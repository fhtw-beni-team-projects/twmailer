#pragma once

#include "mail.h"

#include <string>
#include <set>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

template <typename T>
static const bool ptr_cmp(T* left, T* right) { return *left < *right; };

typedef std::set<mail*, decltype(ptr_cmp<mail*>)*> maillist;

class user {
public:

	user(fs::path user_data_json);
	user(std::string name, fs::path user_dir);
	
	~user();

	void addMail(mail* mail);
	void sendMail(mail* mail, std::vector<std::string> recipients);

	mail* getMail(u_int id);
	maillist getMails() { return this->mails; };
	
private:

	json user_data;

	std::string name;
	maillist mails;

};