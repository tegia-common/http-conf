#include "CONNECTION.h"

////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// IMPORT                                                                                 //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

#define ACTOR_TYPE "HTTP::CONNECTION"


extern "C" tegia::actors::type_base_t * _init_type(const std::string &type_name)
{	
	auto type = new tegia::actors::type_t<HTTP::CONNECTION>(ACTOR_TYPE);

	ADD_ACTION("/init",     &HTTP::CONNECTION::init,     ROLES::SESSION::SYSTEM);
	ADD_ACTION("/response", &HTTP::CONNECTION::response, ROLES::SESSION::SYSTEM, ROLES::SESSION::PUBLIC, ROLES::SESSION::USER);
	ADD_ACTION("/test/01",  &HTTP::CONNECTION::test_01,  ROLES::SESSION::PUBLIC, ROLES::SESSION::USER);
	ADD_ACTION("/test/02",  &HTTP::CONNECTION::test_02,  ROLES::SESSION::PUBLIC, ROLES::SESSION::USER);

	return type;
};


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// CONSTRUCTOR / DESTRUCTOR                                                               //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

namespace HTTP {


CONNECTION::CONNECTION(const std::string &name): tegia::actors::actor_t(ACTOR_TYPE,name)
{
	this->_validator_http_data.load(tegia::conf::path("http-conf") + "/data/schemas/http_data.json");
	this->status = 200;
};

CONNECTION::~CONNECTION() { };

}	// END namespace HTTP


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// ACTION FUNCTIONS                                                                       //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////


#include "actions/init.cpp"
#include "actions/response.cpp"
#include "actions/test.cpp"

	
////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// COMMON FUNCTIONS                                                                       //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

#include "common/route_send.cpp"
#include "common/route_current.cpp"
#include "common/route_jwt_public_key.cpp"

