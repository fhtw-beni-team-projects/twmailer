#pragma once

#include "mail.h"

#include <string>
#include <set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

template <typename  T>
static const bool ptr_cmp = [](T* left, T* right) { return *left < *right; };

typedef  std::set<mail*, decltype(ptr_cmp<int>)> maillist;

class user {
public:

	user(fs::path);
	user(std::string name)
	: name(name) {};
	
	~user();

	void addMail(mail mail);
	maillist getMails() { return this->mails; };
	
private:

	const std::string name;
	maillist mails;

};