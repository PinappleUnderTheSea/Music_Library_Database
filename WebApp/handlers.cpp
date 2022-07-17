#include "handlers.h"

#include <vector>

#include "rendering.h"

// register an orm mapping (to convert the db query results into
// json objects).
// the db query results contain several rows, each has a number of
// fields. the order of `make_db_field<Type[i]>(name[i])` in the
// initializer list corresponds to these fields (`Type[0]` and
// `name[0]` correspond to field[0], `Type[1]` and `name[1]`
// correspond to field[1], ...). `Type[i]` is the type you want
// to convert the field value to, and `name[i]` is the identifier
// with which you want to store the field in the json object, so
// if the returned json object is `obj`, `obj[name[i]]` will have
// the type of `Type[i]` and store the value of field[i].
bserv::db_relation_to_object orm_user{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("username"),
	bserv::make_db_field<std::string>("password"),
	bserv::make_db_field<bool>("is_superuser"),
	bserv::make_db_field<std::string>("first_name"),
	bserv::make_db_field<std::string>("last_name"),
	bserv::make_db_field<std::string>("email"),
	bserv::make_db_field<bool>("is_active")
};

bserv::db_relation_to_object orm_list{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("musicname"),
	bserv::make_db_field<int>("length"),
	bserv::make_db_field<int>("year"),
	bserv::make_db_field<std::string>("language"),
	bserv::make_db_field<std::string>("sname"),

};

bserv::db_relation_to_object orm_language{
	bserv::make_db_field<std::string>("language"),
};

bserv::db_relation_to_object orm_singer{
	bserv::make_db_field<std::string>("sname"),
};

bserv::db_relation_to_object orm_collection{
	bserv::make_db_field<std::string>("mname"),
	bserv::make_db_field<std::string>("sname"),
	bserv::make_db_field<std::string>("uname"),
	bserv::make_db_field<int>("freq"),
	bserv::make_db_field<bool>("is_favorite"),
};
bserv::db_relation_to_object orm_more{
	bserv::make_db_field<std::string>("sname"),
	bserv::make_db_field<std::string>("sex"),
	bserv::make_db_field<int>("birthyear"),
	bserv::make_db_field<std::string>("area"),
	bserv::make_db_field<std::string>("message"),
	bserv::make_db_field<std::string>("award"),
};

bserv::db_relation_to_object orm_song{
	bserv::make_db_field<std::string>("musicname"),

};

bserv::db_relation_to_object orm_collect{
	bserv::make_db_field<std::string>("mname"),
	bserv::make_db_field<std::string>("uname"),
	bserv::make_db_field<int>("freq"),
	bserv::make_db_field<bool>("is_favorite"),

};

std::optional<boost::json::object> get_user(
	bserv::db_transaction& tx,
	const boost::json::string& username) {
	bserv::db_result r = tx.exec(
		"select * from auth_user where username = ?", username);
	lginfo << r.query(); // this is how you log info
	return orm_user.convert_to_optional(r);
}

std::optional<boost::json::object> get_list(
	bserv::db_transaction& tx,
	const boost::json::string& musicname) {
	bserv::db_result r = tx.exec(
		"select * from music where musicname = ?", musicname);
	lginfo << r.query(); // this is how you log info
	return orm_list.convert_to_optional(r);
}

std::optional<boost::json::object> get_singer(
	bserv::db_transaction& tx,
	const boost::json::string& sname) {
	bserv::db_result r = tx.exec(
		"select sname from singers where sname = ?", sname);
	lginfo << r.query(); // this is how you log info
	return orm_singer.convert_to_optional(r);
}

std::optional<boost::json::object> get_collect(
	bserv::db_transaction& tx,
	const boost::json::string& musicname,
	const boost::json::string& username) {
	bserv::db_result r = tx.exec(
		"select * from collection where mname = ? and uname=?", musicname,username);
	lginfo << r.query(); // this is how you log info
	return orm_collect.convert_to_optional(r);
}

std::string get_or_empty(
	boost::json::object& obj,
	const std::string& key) {
	return obj.count(key) ? obj[key].as_string().c_str() : "";
}

std::string get_or_zero(
	boost::json::object& obj,
	const std::string& key) {
	return obj.count(key) ? obj[key].as_string().c_str() : 0;
}

// if you want to manually modify the response,
// the return type should be `std::nullopt_t`,
// and the return value should be `std::nullopt`.
std::nullopt_t hello(
	bserv::response_type& response,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	boost::json::object obj;
	if (session.count("user")) {
		// NOTE: modifications to sessions must be performed
		// BEFORE referencing objects in them. this is because
		// modifications might invalidate referenced objects.
		// in this example, "count" might be added to `session`,
		// which should be performed first.
		// then `user` can be referenced safely.
		if (!session.count("count")) {
			session["count"] = 0;
		}
		auto& user = session["user"].as_object();
		session["count"] = session["count"].as_int64() + 1;
		obj = {
			{"welcome", user["username"]},
			{"count", session["count"]}
		};
	}
	else {
		obj = { {"msg", "hello, world!"} };
	}
	// the response body is a string,
	// so the `obj` should be serialized
	response.body() = boost::json::serialize(obj);
	response.prepare_payload(); // this line is important!
	return std::nullopt;
}

// if you return a json object, the serialization
// is performed automatically.
boost::json::object user_register(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("username") == 0) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}
	if (params.count("password") == 0) {
		return {
			{"success", false},
			{"message", "`password` is required"}
		};
	}
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`username` existed"}
		};
	}
	auto password = params["password"].as_string();
	bserv::db_result r = tx.exec(
		"insert into ? "
		"(?, password, is_superuser, "
		"first_name, last_name, email, is_active) values "
		"(?, ?, ?, ?, ?, ?, ?)", bserv::db_name("auth_user"),
		bserv::db_name("username"),
		username,
		bserv::utils::security::encode_password(
			password.c_str()), 
		(get_or_empty(params,"superuser")=="true")?true:false,
		get_or_empty(params, "first_name"),
		get_or_empty(params, "last_name"),
		get_or_empty(params, "email"), true);
	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "user registered"}
	};
}

boost::json::object user_delete(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("duser") == 0) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}
	if (params.count("password") == 0) {
		return {
			{"success", false},
			{"message", "`password` is required"}
		};
	}
	auto username = params["duser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`username` does not exist"}
		};
	}
	auto& user = opt_user.value();
	if (!user["is_active"].as_bool()) {
		return {
			{"success", false},
			{"message", "invalid username/password"}
		};
	}
	auto password = params["password"].as_string();
	auto encoded_password = user["password"].as_string();
	if (!bserv::utils::security::check_password(
		password.c_str(), encoded_password.c_str())) {
		return {
			{"success", false},
			{"message", "invalid username/password"}
		};
	}
	bserv::db_result r = tx.exec(
		"update ?"
		"set is_active=false "
		"where ?=? ",
		 bserv::db_name("auth_user"),
		bserv::db_name("username"),
		username);
	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "user deleted"}
	};
}

boost::json::object list_delete(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("colname") == 0) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}

	auto mname = params["colname"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_list(tx, mname);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` does not exist"}
		};
	}

	bserv::db_result r = tx.exec(
		"delete from ?"
		"where mname=? ",
		bserv::db_name("collection"),
		get_or_empty(params, "colname"));

	 r = tx.exec(
		"delete from ?"
		"where musicname=? ",
		bserv::db_name("music"),
		get_or_empty(params,"colname"));
	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "music deleted"}
	};
}

boost::json::object list_register(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("musicname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"] ;
	auto musicname = params["musicname"].as_string();

	bserv::db_transaction tx{ conn };
	auto opt_user = get_list(tx, musicname);
	if (opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`musicname` existed"}
		};
	}
	if (params["musicname"].as_string().size() == 0 || params["legnth"].as_string().size() == 0 || params["year"].as_string().size() == 0 || params["langauge"].as_string().size() == 0 || params["sname"].as_string().size() == 0) {
		return{
			{"seccess",false},
			{"message","Please fill the table"}
		};
	}
	bserv::db_result r = tx.exec(
		"insert into ? "
		"(?, ?, "
		"?, ?, ?) values "
		"(?, ?, ?, ?, ? )", bserv::db_name("music"),
		bserv::db_name("musicname"),
		bserv::db_name("length"),
		bserv::db_name("year"),
		bserv::db_name("language"),
		bserv::db_name("sname"),
		//musicname,
		get_or_empty(params, "musicname"),
		
		get_or_zero(params, "length"),
		get_or_zero(params, "year"),
		get_or_empty(params, "language"), 
		get_or_empty(params, "sname")
		);
	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "list registered"}
	};
}
boost::json::object singer_register(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("addsname") == 0) {
		return {
			{"success", false},
			{"message", "`SingerName` is required"}
		};
	}
	if (params["birthyear"].as_string().size() == 0 || params["sex"].as_string().size() == 0 || params["message"].as_string().size() == 0 || params["award"].as_string().size() == 0 || params["sname"].as_string().size() == 0 || params["area"].as_string().size() == 0) {
		return{
			{"seccess",false},
			{"message","Please fill the table"}
		};
	}
	lgdebug << params["language"];
	auto sname = params["addsname"].as_string();

	bserv::db_transaction tx{ conn };
	auto opt_user = get_singer(tx, sname);
	if (opt_user.has_value()) {
		bserv::db_result r = tx.exec(
			"update ? "
			"set ?=? ,?=?, ?=?, ?=?, ?=?  "
			"where ?=?",
			bserv::db_name("singers"),
			
			bserv::db_name("birthyear"),
			get_or_zero(params, "addbirthyear"),
			bserv::db_name("sex"),
			get_or_zero(params, "addsex"),
			bserv::db_name("message"),
			get_or_empty(params, "message"),
			bserv::db_name("award"),
			get_or_empty(params, "award"),
			bserv::db_name("area"),
			get_or_empty(params, "area"),
			bserv::db_name("sname"),
			//musicname,
			get_or_empty(params, "addsname")

			
		);
	}
	else {
		bserv::db_result r = tx.exec(
			"insert into ? "
			"(?, ?, "
			"?, ?, ?,?) values "
			"(?, ?, ?, ?, ?,? )", bserv::db_name("singers"),
			bserv::db_name("sname"),
			bserv::db_name("birthyear"),
			bserv::db_name("sex"),
			bserv::db_name("message"),
			bserv::db_name("award"),
			bserv::db_name("area"),
			//musicname,
			get_or_empty(params, "addsname"),

			get_or_zero(params, "addbirthyear"),
			get_or_zero(params, "addsex"),
			get_or_empty(params, "message"),
			get_or_empty(params, "award"),
			get_or_empty(params, "area")
		);
	}
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "singer registered"}
	};
}
boost::json::object list_collect(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("colname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"];
	auto musicname = params["colname"].as_string();
	auto username = params["coluser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_collect(tx, musicname,username);
	if (opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` has been collected"}
		};
	}
	bserv::db_result r = tx.exec(
		"insert into ? "
		"(?, ?, "
		"?, ?) values "
		"(?, ?, ?, ? )", bserv::db_name("collection"),
		bserv::db_name("uname"),
		bserv::db_name("mname"),
		bserv::db_name("freq"),
		bserv::db_name("is_favorite"),
		//musicname,
		get_or_empty(params, "coluser"),

		get_or_empty(params, "colname"),
		0,
		false);


	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "music collected"}
	};
}

boost::json::object favor_set(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("setname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"];
	auto musicname = params["setname"].as_string();
	auto username = params["setuser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_collect(tx, musicname, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` has not been collected"}
		};
	}
	bserv::db_result r = tx.exec(
		"update  ? "
		"set is_favorite=not is_favorite "
		"where mname=? and uname=? ",
		bserv::db_name("collection"),

		//musicname,
		get_or_empty(params, "setname"),

		get_or_empty(params, "setuser")
		
		);


	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "Favor set"}
	};
}

boost::json::object play_set(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("setname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"];
	auto musicname = params["setname"].as_string();
	auto username = params["setuser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_collect(tx, musicname, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` has not been collected"}
		};
	}
	bserv::db_result r = tx.exec(
		"update  ? "
		"set freq =freq+1 "
		"where mname=? and uname=? ",
		bserv::db_name("collection"),

		//musicname,
		get_or_empty(params, "setname"),

		get_or_empty(params, "setuser")

	);


	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "Ready to Play"}
	};
}

boost::json::object clear_set(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("setname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"];
	auto musicname = params["setname"].as_string();
	auto username = params["setuser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_collect(tx, musicname, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` has not been collected"}
		};
	}
	bserv::db_result r = tx.exec(
		"update  ? "
		"set freq =0 "
		"where mname=? and uname=? ",
		bserv::db_name("collection"),

		//musicname,
		get_or_empty(params, "setname"),

		get_or_empty(params, "setuser")

	);


	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "Clear History"}
	};
}

boost::json::object delete_set(
	bserv::request_type& request,
	// the json object is obtained from the request body,
	// as well as the url parameters
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("setname") == 0) {
		return {
			{"success", false},
			{"message", "`musicname` is required"}
		};
	}
	lgdebug << params["language"];
	auto musicname = params["setname"].as_string();
	auto username = params["setuser"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_collect(tx, musicname, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "`music` has not been collected"}
		};
	}
	bserv::db_result r = tx.exec(
		"delete from  ? "
		"where mname=? and uname=? ",
		bserv::db_name("collection"),

		//musicname,
		get_or_empty(params, "setname"),

		get_or_empty(params, "setuser")

	);


	lginfo << r.query();
	tx.commit(); // you must manually commit changes
	return {
		{"success", true},
		{"message", "Delete Succesfully"}
	};
}

boost::json::object user_login(
	bserv::request_type& request,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	if (request.method() != boost::beast::http::verb::post) {
		throw bserv::url_not_found_exception{};
	}
	if (params.count("username") == 0) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}
	if (params.count("password") == 0) {
		return {
			{"success", false},
			{"message", "`password` is required"}
		};
	}
	auto username = params["username"].as_string();
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (!opt_user.has_value()) {
		return {
			{"success", false},
			{"message", "invalid username/password"}
		};
	}
	auto& user = opt_user.value();
	if (!user["is_active"].as_bool()) {
		return {
			{"success", false},
			{"message", "invalid username/password"}
		};
	}
	auto password = params["password"].as_string();
	auto encoded_password = user["password"].as_string();
	if (!bserv::utils::security::check_password(
		password.c_str(), encoded_password.c_str())) {
		return {
			{"success", false},
			{"message", "invalid username/password"}
		};
	}
	bserv::session_type& session = *session_ptr;
	session["user"] = user;
	return {
		{"success", true},
		{"message", "login successfully"}
	};
}

boost::json::object find_user(
	std::shared_ptr<bserv::db_connection> conn,
	const std::string& username) {
	bserv::db_transaction tx{ conn };
	auto user = get_user(tx, username.c_str());
	if (!user.has_value()) {
		return {
			{"success", false},
			{"message", "requested user does not exist"}
		};
	}
	user.value().erase("id");
	user.value().erase("password");
	return {
		{"success", true},
		{"user", user.value()}
	};
}

boost::json::object user_logout(
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (session.count("user")) {
		session.erase("user");
	}
	return {
		{"success", true},
		{"message", "logout successfully"}
	};
}

boost::json::object send_request(
	std::shared_ptr<bserv::session_type> session,
	std::shared_ptr<bserv::http_client> client_ptr,
	boost::json::object&& params) {
	// post for response:
	// auto res = client_ptr->post(
	//     "localhost", "8080", "/echo", {{"msg", "request"}}
	// );
	// return {{"response", boost::json::parse(res.body())}};
	// -------------------------------------------------------
	// - if it takes longer than 30 seconds (by default) to
	// - get the response, this will raise a read timeout
	// -------------------------------------------------------
	// post for json response (json value, rather than json
	// object, is returned):
	auto obj = client_ptr->post_for_value(
		"localhost", "8080", "/echo", { {"request", params} }
	);
	if (session->count("cnt") == 0) {
		(*session)["cnt"] = 0;
	}
	(*session)["cnt"] = (*session)["cnt"].as_int64() + 1;
	return { {"response", obj}, {"cnt", (*session)["cnt"]} };
}

boost::json::object echo(
	boost::json::object&& params) {
	return { {"echo", params} };
}

// websocket
std::nullopt_t ws_echo(
	std::shared_ptr<bserv::session_type> session,
	std::shared_ptr<bserv::websocket_server> ws_server) {
	ws_server->write_json((*session)["cnt"]);
	while (true) {
		try {
			std::string data = ws_server->read();
			ws_server->write(data);
		}
		catch (bserv::websocket_closed&) {
			break;
		}
	}
	return std::nullopt;
}


std::nullopt_t serve_static_files(
	bserv::response_type& response,
	const std::string& path) {
	return serve(response, path);
}


std::nullopt_t index(
	const std::string& template_path,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object& context) {
	bserv::session_type& session = *session_ptr;
	if (session.contains("user")) {
		context["user"] = session["user"];
	}
	return render(response, template_path, context);
}

/*std::nullopt_t index_list(
	const std::string& template_path,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object& context) {
	bserv::session_type& session = *session_ptr;
	if (session.contains("user")) {
		context["user"] = session["uesr"];
	}
	return render(response, template_path, context);
}*/

std::nullopt_t index_page(
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	boost::json::object context;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t form_login(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	lgdebug << params << std::endl;
	auto context = user_login(request, std::move(params), conn, session_ptr);
	lginfo << "login: " << context << std::endl;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t form_logout(
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response) {
	auto context = user_logout(session_ptr);
	lginfo << "logout: " << context << std::endl;
	return index("index.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_users(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from auth_user;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from auth_user where is_active=true limit 10 offset ?;", (page_id - 1) * 10);
	lginfo << db_res.query();
	auto users = orm_user.convert_to_vector(db_res);
	boost::json::array json_users;
	for (auto& user : users) {
		json_users.push_back(user);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["users"] = json_users;
	return index("users.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_list(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context) {
	lgdebug << "view users: " << page_id << std::endl;
	
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from music;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;	
	
	db_res = tx.exec("select * from music limit 10 offset ?;", (page_id - 1) * 10);
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& list : lists) {
		json_lists.push_back(list);
	}
	db_res = tx.exec("select distinct language from music");
	auto languages = orm_language.convert_to_vector(db_res);
	boost::json::array json_languages;
	for (auto& language : languages) {
		json_languages.push_back(language);
	}
	db_res = tx.exec("select distinct sname from music");
	auto singers = orm_singer.convert_to_vector(db_res);
	boost::json::array json_singers;
	for (auto& singer : singers) {
		json_singers.push_back(singer);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["lists"] = json_lists;
	context["languages"] = json_languages;
	context["singers"] = json_singers;
	return index("list.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_collection(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;

	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from collection;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	bserv::session_type& session = *session_ptr;
	db_res = tx.exec("select mname,sname,uname,freq,is_favorite from collection,music where mname=musicname and uname=? order by freq desc ;", params["setuser"].as_string());
	lginfo << db_res.query();
	auto collections = orm_collection.convert_to_vector(db_res);
	boost::json::array json_collections;
	for (auto& collection : collections) {
		json_collections.push_back(collection);
	}
	
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["collections"] = json_collections;
	return index("collection.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_more(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;

	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from collection;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	bserv::session_type& session = *session_ptr;
	db_res = tx.exec("select sname,sex,birthyear,area,message,award from singers where sname=?;", params["colsinger"].as_string());
	lginfo << db_res.query();
	auto mores = orm_more.convert_to_vector(db_res);
	boost::json::array json_mores;
	for (auto& collection : mores) {
		json_mores.push_back(collection);
	}
	db_res = tx.exec("select musicname from music where sname=?;", params["colsinger"].as_string());
	auto songs = orm_song.convert_to_vector(db_res);
	boost::json::array json_songs;
	for (auto& collection : songs) {
		json_songs.push_back(collection);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["mores"] = json_mores;
	context["songs"] = json_songs;
	return index("more.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_rec(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;

	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from collection;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	bserv::session_type& session = *session_ptr;
	db_res = tx.exec("select x.id,x.musicname,x.length,x.year,x.language,x.sname from music as x where x.sname in (select y.sname from music as y,collection as z where z.uname=? and y.musicname=z.mname group by y.sname having sum(z.freq)>=all (select sum (freq) from music,collection where music.musicname=collection.mname and collection.uname=? group by music.sname) );"
		, get_or_empty(params,"setuser"), get_or_empty(params,"setuser"));
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& collection : lists) {
		json_lists.push_back(collection);
	}
	db_res = tx.exec("select y.sname from music as y,collection as z where z.uname=? and y.musicname=z.mname group by y.sname having sum(z.freq)>=all (select sum (freq) from music,collection where music.musicname=collection.mname and collection.uname=? group by music.sname) limit 10 offset ?;"
		, params["setuser"].as_string(), params["setuser"].as_string(), (page_id - 1) * 10);
	auto lists1 = orm_singer.convert_to_vector(db_res);
	boost::json::array json_lists1;
	for (auto& collection : lists1) {
		json_lists1.push_back(collection);
	}
	
	db_res = tx.exec("select x.id,x.musicname,x.length,x.year,x.language,x.sname from music as x where x.language in (select y.language from music as y,collection as z where z.uname=? and y.musicname=z.mname group by y.language having sum(z.freq)>=all (select sum (freq) from music,collection where music.musicname=collection.mname and collection.uname=? group by music.language) )limit 10 offset ?;"
		, params["setuser"].as_string(), params["setuser"].as_string(), (page_id - 1) * 10);
	auto lists2 = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists2;
	for (auto& collection : lists2) {
		json_lists2.push_back(collection);
	}

	db_res = tx.exec("select y.language from music as y,collection as z where z.uname=? and y.musicname=z.mname group by y.language having sum(z.freq)>=all (select sum (freq) from music,collection where music.musicname=collection.mname and collection.uname=? group by music.language)		limit 10 offset ?;"
		, params["setuser"].as_string(), params["setuser"].as_string(), (page_id - 1) * 10);
	auto lists3 = orm_language.convert_to_vector(db_res);
	boost::json::array json_lists3;
	for (auto& collection : lists3) {
		json_lists3.push_back(collection);
	}
	db_res = tx.exec("select x.id,x.musicname,x.length,x.year,x.language,x.sname from music as x ,collection as y where x.musicname=y.mname and y.is_favorite=true and y.uname=? limit 10 offset ?;"
		, params["setuser"].as_string(),  (page_id - 1) * 10);
	auto lists4 = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists4;
	for (auto& collection : lists4) {
		json_lists4.push_back(collection);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["rec1s"] = json_lists;
	context["rec1_singer"] = json_lists1;
	context["rec2s"] = json_lists2;
	context["rec2_language"] = json_lists3;
	context["rec3s"] = json_lists4;
	return index("recommandation.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_language(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from music ;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from music where language=? limit 10 offset ? ;",params["languageback"].as_string(), (page_id - 1) * 10);
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& list : lists) {
		json_lists.push_back(list);
	}
	db_res = tx.exec("select distinct language from music");
	auto languages = orm_language.convert_to_vector(db_res);
	boost::json::array json_languages;
	for (auto& language : languages) {
		json_languages.push_back(language);
	}
	db_res = tx.exec("select distinct sname from music");
	auto singers = orm_singer.convert_to_vector(db_res);
	boost::json::array json_singers;
	for (auto& singer : singers) {
		json_singers.push_back(singer);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["lists"] = json_lists;
	context["languages"] = json_languages;
	context["singers"] = json_singers;
	return index("list.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_search(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from music ;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from music where musicname=? limit 10 offset ? ;", params["search"].as_string(), (page_id - 1) * 10);
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& list : lists) {
		json_lists.push_back(list);
	}
	db_res = tx.exec("select distinct language from music");
	auto languages = orm_language.convert_to_vector(db_res);
	boost::json::array json_languages;
	for (auto& language : languages) {
		json_languages.push_back(language);
	}
	db_res = tx.exec("select distinct sname from music");
	auto singers = orm_singer.convert_to_vector(db_res);
	boost::json::array json_singers;
	for (auto& singer : singers) {
		json_singers.push_back(singer);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["lists"] = json_lists;
	context["languages"] = json_languages;
	context["singers"] = json_singers;
	return index("list.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_searchs(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from music ;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from music where sname=? limit 10 offset ? ;", params["searchs"].as_string(), (page_id - 1) * 10);
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& list : lists) {
		json_lists.push_back(list);
	}
	db_res = tx.exec("select distinct language from music");
	auto languages = orm_language.convert_to_vector(db_res);
	boost::json::array json_languages;
	for (auto& language : languages) {
		json_languages.push_back(language);
	}
	db_res = tx.exec("select distinct sname from music");
	auto singers = orm_singer.convert_to_vector(db_res);
	boost::json::array json_singers;
	for (auto& singer : singers) {
		json_singers.push_back(singer);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["lists"] = json_lists;
	context["languages"] = json_languages;
	context["singers"] = json_singers;
	return index("list.html", session_ptr, response, context);
}

std::nullopt_t redirect_to_singer(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	int page_id,
	boost::json::object&& context,
	boost::json::object&& params) {
	lgdebug << "view users: " << page_id << std::endl;
	bserv::db_transaction tx{ conn };
	bserv::db_result db_res = tx.exec("select count(*) from music ;");
	lginfo << db_res.query();
	std::size_t total_users = (*db_res.begin())[0].as<std::size_t>();
	lgdebug << "total users: " << total_users << std::endl;
	int total_pages = (int)total_users / 10;
	if (total_users % 10 != 0) ++total_pages;
	lgdebug << "total pages: " << total_pages << std::endl;
	db_res = tx.exec("select * from music where sname=? limit 10 offset ? ;", params["singerback"].as_string(), (page_id - 1) * 10);
	lginfo << db_res.query();
	auto lists = orm_list.convert_to_vector(db_res);
	boost::json::array json_lists;
	for (auto& list : lists) {
		json_lists.push_back(list);
	}
	db_res = tx.exec("select distinct sname from music");
	auto singers = orm_singer.convert_to_vector(db_res);
	boost::json::array json_singers;
	for (auto& singer : singers) {
		json_singers.push_back(singer);
	}
	lgdebug << json_singers.size();
	db_res = tx.exec("select distinct language from music");
	auto languages = orm_language.convert_to_vector(db_res);
	boost::json::array json_languages;
	for (auto& language : languages) {
		json_languages.push_back(language);
	}
	boost::json::object pagination;
	if (total_pages != 0) {
		pagination["total"] = total_pages;
		if (page_id > 1) {
			pagination["previous"] = page_id - 1;
		}
		if (page_id < total_pages) {
			pagination["next"] = page_id + 1;
		}
		int lower = page_id - 3;
		int upper = page_id + 3;
		if (page_id - 3 > 2) {
			pagination["left_ellipsis"] = true;
		}
		else {
			lower = 1;
		}
		if (page_id + 3 < total_pages - 1) {
			pagination["right_ellipsis"] = true;
		}
		else {
			upper = total_pages;
		}
		pagination["current"] = page_id;
		boost::json::array pages_left;
		for (int i = lower; i < page_id; ++i) {
			pages_left.push_back(i);
		}
		pagination["pages_left"] = pages_left;
		boost::json::array pages_right;
		for (int i = page_id + 1; i <= upper; ++i) {
			pages_right.push_back(i);
		}
		pagination["pages_right"] = pages_right;
		context["pagination"] = pagination;
	}
	context["lists"] = json_lists;
	context["singers"] = json_singers;
	context["languages"] = json_languages;
	return index("list.html", session_ptr, response, context);
}

std::nullopt_t view_users(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_users(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t view_list(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_list(conn, session_ptr, response, page_id, std::move(context));
}

std::nullopt_t view_collection(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_collection(conn, session_ptr, response, page_id, std::move(context), std::move(params));
}

std::nullopt_t view_rec(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_rec(conn, session_ptr, response, page_id, std::move(context), std::move(params));
}

std::nullopt_t view_language(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_language(conn, session_ptr, response, page_id, std::move(context),std::move(params));
}

std::nullopt_t view_search(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_search(conn, session_ptr, response, page_id, std::move(context), std::move(params));
}

std::nullopt_t view_searchs(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_searchs(conn, session_ptr, response, page_id, std::move(context), std::move(params));
}

std::nullopt_t view_singer(
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr,
	bserv::response_type& response,
	boost::json::object&& params,
	const std::string& page_num) {
	int page_id = std::stoi(page_num);
	boost::json::object context;
	return redirect_to_singer(conn, session_ptr, response, page_id, std::move(context), std::move(params));
}

std::nullopt_t form_add_user(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = user_register(request, std::move(params), conn);
	return redirect_to_users(conn, session_ptr, response, 1, std::move(context));
}
std::nullopt_t form_add_list(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = list_register(request, std::move(params), conn);
	return redirect_to_list(conn, session_ptr, response, 1, std::move(context));
}
std::nullopt_t form_add_singer(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = singer_register(request, std::move(params), conn);
	return redirect_to_list(conn, session_ptr, response, 1, std::move(context));
}
std::nullopt_t delete_user(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = user_delete(request, std::move(params), conn);
	return redirect_to_users(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t delete_list(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = list_delete(request, std::move(params), conn);
	return redirect_to_list(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t collect(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = list_collect(request, std::move(params), conn);
	return redirect_to_list(conn, session_ptr, response, 1, std::move(context));
}

std::nullopt_t set_favor(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = favor_set(request, std::move(params), conn);
	return redirect_to_collection(conn, session_ptr, response, 1, std::move(context), std::move(params));
}

std::nullopt_t view_more(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context ;
	return redirect_to_more(conn, session_ptr, response, 1, std::move(context), std::move(params));
}

std::nullopt_t play(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = play_set(request, std::move(params), conn);
	return redirect_to_collection(conn, session_ptr, response, 1, std::move(context), std::move(params));
}

std::nullopt_t clear(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = clear_set(request, std::move(params), conn);
	return redirect_to_collection(conn, session_ptr, response, 1, std::move(context), std::move(params));
}

std::nullopt_t dlt(
	bserv::request_type& request,
	bserv::response_type& response,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn,
	std::shared_ptr<bserv::session_type> session_ptr) {
	boost::json::object context = delete_set(request, std::move(params), conn);
	return redirect_to_collection(conn, session_ptr, response, 1, std::move(context), std::move(params));
}