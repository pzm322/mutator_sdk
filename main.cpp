#include "mutator/inst.hpp"

void on_mmap_end( void* this_ptr ) {
    printf( "PE mapped!\n" );
}

void on_export_init( void* this_ptr ) {
    auto details = reinterpret_cast< pzm::export_callback_t* >( this_ptr );

    // since mapper can't know what's the actual size of exported var you need to set it yourself
    // to determine which export you're dealing with, access the "name" field in details ptr
    // in this sample we consider that all of our exports are 16 bytes length
    // note: default size is always 4
    details->set_size( 16 );

    // predefine all exports starting with the "offset_" prefix
    if ( details->name.substr( 0, 7 ) == "offset_" ) {
        uint32_t offset_value = 0x12345;
        details->data = &offset_value;

        // data field should contain a pointer to the actual data
    } else {
        details->set_size( sizeof( uint16_t ) );

        uint16_t some_value = 0x1337;
        details->data = &some_value;
    }

    printf( "[init] export - %s\n", details->name.c_str( ) );
}

void on_export_mmap( void* this_ptr ) {
    auto details = reinterpret_cast< pzm::export_callback_t* >( this_ptr );
    printf( "[mmap] export - %s\n", details->name.c_str( ) );
}

int main( ) {
    printf( "hello, sample!\n" );

    if ( !pzm::setup( ) ) {
        printf( "failed to setup instance\n" );
        return 1;
    }

    pzm::username = "username"; // your username from website
    pzm::password = "password"; // your password from website

    if ( !pzm::auth( ) ) {
        printf( "failed to auth, check your credentials and subscription\n" );
        return 2;
    }

    // create a new directory inside a folder with executable
    // place both of files (pe and map) inside that directory
    // pass the directory name as a string argument to set_directory function
    pzm::instance->set_directory( "test" );

    // you can add several callbacks by calling add_callback function
    // every callback handler will be executed once callback conditions are reached
    // for detailed documentation please refer to our website
    pzm::instance->add_callback( pzm::callback_t::CALLBACK_EXPORT_INIT, on_export_init );
    pzm::instance->add_callback( pzm::callback_t::CALLBACK_EXPORT_MMAP, on_export_mmap );

    // configuring for mutator is available via set_option function
    // you should pass option value type as a template parameter
    // then you'll need to pass option itself and value for it
    pzm::instance->set_option< bool >( pzm::option_t::OPTION_SHUFFLE, true );
    pzm::instance->set_option< bool >( pzm::option_t::OPTION_PARTITION, true );

    auto status = pzm::instance->initialize( );
    if ( status != pzm::status_t::STATUS_SUCCESS ) {
        printf( "error (code %d) occurred!\n", status );
        pzm::unload( );
        return 3;
    }

    printf( "mutator has been successfully initialized!\n" );

    // mapper data is a json object that contains all necessary information your binary requires
    // there's a size field which is responsible for allocation size and imports that are being used
    // mapper data contains a "client_id" field which is required for further operations
    auto mapper_data = pzm::instance->get_mapper_data( );
    auto client_id = mapper_data.at( "client_id" ).get< uint32_t >( );

    printf( "[+] mapper data - %s (client_id - 0x%X)\n", mapper_data.dump( ).c_str( ), client_id );

    nlohmann::json client_info;
    client_info[ "client_id" ] = client_id;
    for ( const auto& region_size : mapper_data.at( "sizes" ).get< nlohmann::json::array_t >( ) )
        client_info[ "bases" ].emplace_back( 0x10000000 ); // base of allocated region

    // these imports should be resolved on client
    // after obtaining import addresses pass them to json object in sequence : module -> function -> address
    for ( const auto& imported_module : mapper_data.at( "imports" ).get< nlohmann::json::object_t >( ) ) {
        for ( const auto& function : imported_module.second.get< nlohmann::json::array_t >( ) )
            client_info[ "imports" ][ imported_module.first ][ function.get< std::string >( ) ] = 0x77000000;
    }

    // for now binary is already mutated and ready to be mapped and launched
    // create an std::vector< uint8_t > which will later contain entire PE binary
    // pass client information and reference to binary storage as arguments in proceed function
    std::vector< std::vector< uint8_t > > mapped_binaries = { };
    if ( !pzm::instance->proceed( client_info, mapped_binaries ) ) {
        printf( "failed to proceed binary mapping!\n" );
    }

    printf( "[+] launch details - %s\n", client_info.dump( ).c_str( ) );

    std::ofstream test_binary( "test_file.bin", std::ios::binary );
    test_binary.write( ( char* ) mapped_binaries.at( 0 ).data( ),
        ( long long ) mapped_binaries.at( 0 ).size( ) );
    test_binary.close( );

    pzm::unload( );
    return 0;
}