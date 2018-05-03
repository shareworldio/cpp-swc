#pragma once

#include <libsolidity/Node.h>
#include <string>

std::string compileFile(std::string _name, std::string _contract_name);
std::string compileCode(std::string _name, std::string _code);
std::string compileNode();
std::string compileNodeAbi();
std::string nodeAddress();


