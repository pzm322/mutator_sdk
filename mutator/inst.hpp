#include <iostream>
#include <unordered_map>
#include <functional>
#include <deque>
#include <fstream>
#include <filesystem>

#include "helpers/ws/server_wss.hpp"
#include "helpers/ws/client_wss.hpp"

#include "helpers/json.hpp"

namespace pzm {
    using ws_client_t = SimpleWeb::SocketClient< SimpleWeb::WSS >;

    struct ws_packet_t {
        size_t packet_id = 0;
        nlohmann::json content;
    };

    int request_status = -1;

    std::string username;
    std::string password;

    std::string session_id;

    std::thread g_client_thread;

    std::deque< ws_packet_t > received_packets = { };
    std::shared_ptr< ws_client_t::Connection > g_connection = { };

    ws_client_t g_client( "fatal.shop:443/ws/", false );

    enum class callback_t {
        CALLBACK_EXPORT_INIT = 0,
        CALLBACK_EXPORT_MMAP
    };

    enum class option_t {
        OPTION_SHUFFLE = 0,
    };

    enum class status_t {
        STATUS_SUCCESS = 0,
        STATUS_INVALID_FILE,
        STATUS_MISSING_MAP,
        STATUS_MISSING_BIN,
        STATUS_INVALID_BIN
    };

    class c_mutator {

        public:

            void set_directory( const std::string& directory_path ) {
                if ( !std::filesystem::is_directory( directory_path ) ) {
                    last_status = status_t::STATUS_INVALID_FILE;
                    return;
                }

                for ( const auto& entry : std::filesystem::directory_iterator( directory_path ) ) {
                    auto extension = entry.path( ).extension( ).string( );
                    if ( extension != ".map" && extension != ".dll" )
                        continue;

                    if ( extension == ".map" ) {
                        std::stringstream map_stream;
                        std::ifstream map_file_stream( entry.path( ).string( ) );
                        map_stream << map_file_stream.rdbuf( );
                        map_file_stream.close( );

                        map_file = map_stream.str( );
                    } else {
                        std::ifstream bin_stream( entry.path( ).string( ), std::ios::binary );
                        bin_stream.unsetf( std::ios::skipws );
                        bin_stream.seekg( 0, std::ios::end );

                        auto bin_size = static_cast< size_t >( bin_stream.tellg( ) );

                        bin_stream.seekg( 0, std::ios::beg );
                        pe_binary.resize( bin_size );
                        pe_binary.insert( pe_binary.begin( ), std::istream_iterator< uint8_t >( bin_stream ),
                            std::istream_iterator< uint8_t >( ) );
                    }
                }

                if ( map_file.empty( ) ) {
                    last_status = status_t::STATUS_MISSING_MAP;
                    return;
                }

                if ( pe_binary.empty( ) ) {
                    last_status = status_t::STATUS_MISSING_BIN;
                    return;
                }
            }

            void add_callback( callback_t callback, const std::function< void ( void* ) >& handler ) {
                m_callbacks.emplace( std::make_pair( callback, handler ) );
            }

            template< class T >
            void set_option( option_t option, T value ) {
                switch ( option ) {
                    case option_t::OPTION_SHUFFLE:
                        m_options.shuffle = value;
                        break;
                }
            }

            status_t initialize( ) {
                if ( last_status != status_t::STATUS_SUCCESS ) {
                    g_connection->close( );
                    return last_status;
                }

                request_status = -1;
                nlohmann::json init_request;

                init_request[ "session" ] = session_id;
                init_request[ "type" ] = 1;
                init_request[ "map" ] = map_file;
                init_request[ "pe" ] = pe_binary;
                init_request[ "settings" ][ "shuffle" ] = m_options.shuffle;

                g_connection->send( init_request.dump( ) );
                while ( request_status == -1 )
                    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

                return static_cast< status_t >( request_status );
            }

        private:

            std::vector< uint8_t > pe_binary = { };
            std::string map_file;

            std::unordered_map< callback_t, std::function< void ( void* ) > > m_callbacks = { };

            status_t last_status = status_t::STATUS_SUCCESS;

            struct _options {
                bool shuffle = false;
            } m_options;

    };

    std::shared_ptr< c_mutator > instance;

    namespace ws_callbacks {

        void on_message( const std::shared_ptr< ws_client_t::Connection >& connection,
            const std::shared_ptr< ws_client_t::InMessage >& message ) {
            nlohmann::json request;
            try {
                std::stringstream( message->string( ) ) >> request;
            } catch ( ... ) {
                return;
            }

            if ( !request.contains( "type" ) || !request.at( "type" ).is_number_unsigned( ) )
                return;

            switch ( request.at( "type" ).get< size_t >( ) ) {
                case 0: {
                    session_id = request.at( "session_id" ).get< nlohmann::json >( );
                    break;
                }
                case 1:
                case 2: {
                    request_status = request.at( "status" ).get< int >( );
                    break;
                }
            }
        }

    }

    bool setup( ) {
        instance = std::make_shared< c_mutator >( );

        g_client.on_open = [ ] ( const std::shared_ptr< ws_client_t::Connection >& connection ) {
            g_connection = connection;
        };

        g_client.on_message = ws_callbacks::on_message;
        g_client_thread = std::thread( [ ] ( ) -> void {
            g_client.start( );
        } );

        return true;
    }

    bool auth( ) {
        while ( !g_connection )
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        nlohmann::json auth_request;
        auth_request[ "session" ] = session_id;
        auth_request[ "username" ] = username;
        auth_request[ "password" ] = password;
        auth_request[ "type" ] = 0;

        g_connection->send( auth_request.dump( ) );
        while ( request_status == -1 )
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        auto succeeded = ( request_status == 0 );
        request_status = -1;
        return succeeded;
    }

    void unload( ) {
        g_client_thread.detach( );
    }

}