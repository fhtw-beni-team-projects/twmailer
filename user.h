#pragma once

#include "mail.h"

#include <string>
#include <set>
#include <vector>

struct comp {
	bool operator()(mail* left, mail* right) const { return *left < *right; };
};

typedef std::set<mail*, comp> maillist;

class user {
public:

	user(fs::path user_data_json);
	user(std::string name, fs::path user_dir);
	
	~user();

	void addMail(mail* mail);
	void sendMail(mail* mail, std::vector<std::string> recipients);

	mail* getMail(u_int id);
	maillist getMails() { return this->inbox; };

	void saveToFile();
	
private:

	fs::path file_location;
	json user_data;

	std::string name;
	maillist inbox;
	maillist sent;
};