#include "user.h"

#include <fstream>
#include <string>

using json = nlohmann::json;

user::user(fs::path user_data)
{
	// json stuff go here
}

user::user(std::string name, fs::path user_dir)
	: name(name)
{
	json user;
	user["mails"] = json::array();
	user["name"] = name;

	std::ofstream ofs(user_dir/(name+".json"));
	ofs << user;
}

user::~user() {}