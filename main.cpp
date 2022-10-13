#include "mutator/inst.hpp"

void on_export_init( void* this_ptr ) {
    printf( "initing export!\n" );
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

    // configuring for mutator is available via set_option function
    // you should pass option value type as a template parameter
    // then you'll need to pass option itself and value for it
    pzm::instance->set_option< bool >( pzm::option_t::OPTION_SHUFFLE, true );

    auto status = pzm::instance->initialize( );
    if ( status != pzm::status_t::STATUS_SUCCESS ) {
        printf( "error (code %d) occurred!\n", status );
        pzm::unload( );
        return 3;
    }

    printf( "mutator has been successfully initialized!\n" );

    pzm::unload( );
    return 0;
}