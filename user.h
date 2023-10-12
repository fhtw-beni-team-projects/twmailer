#pragma once

#include "mail.h"

#include <string>
#include <set>


template <typename  T>
static const bool ptr_cmp = [](T* left, T* right) { return *left < *right; };

typedef  std::set<mail*, decltype(ptr_cmp<int>)> maillist;

class user {
public:

	user(std::string name, maillist mails)
	: name(name), mails(mails)
	{};
	~user();

	void addMail(mail mail);
	std::set<mail*> getMails();
	
private:

	const std::string name;
	maillist mails;

};