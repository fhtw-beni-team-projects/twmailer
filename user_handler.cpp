#include "user_handler.h"

#include <string>

user_handler::user_handler()
{
	for (const auto& entry : fs::directory_iterator()) {
		if (entry.path().extension() == ".json") {
			this->users.insert(std::pair<std::string, user*>(fs::path(entry.path()).replace_extension(), new user(entry)));
		}
	}
}