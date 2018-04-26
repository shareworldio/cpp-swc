/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Solidity commandline compiler.
 */

#include <clocale>
#include <iostream>
#include <boost/exception/all.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/JSON.h>

#include <libsolidity/interface/StandardCompiler.h>
#include <libsolidity/interface/Version.h>

#include <libsolidity/Solidity.h>

using namespace std;
using namespace dev;
using namespace solidity;

#define NODE_NAME "Node.sol"

std::string readCodeFile(std::string const& _file)
{
	std::string ret;
	size_t const c_elementSize = sizeof(typename std::string::value_type);
	std::ifstream is(_file, std::ifstream::binary);
	if (!is)
		return ret;

	// get length of file:
	is.seekg(0, is.end);
	streamoff length = is.tellg();
	if (length == 0)
		return ret; // do not read empty file (MSVC does not like it)
	is.seekg(0, is.beg);

	ret.resize((length + c_elementSize - 1) / c_elementSize);
	is.read(const_cast<char*>(reinterpret_cast<char const*>(ret.data())), length);
	return ret;
}

std::string compileNodeAbi()
{
	ReadCallback::Callback fileReader = [](string const& _path)
	{
		try
		{
			auto path = boost::filesystem::path(_path);
			auto canonicalPath = boost::filesystem::canonical(path);
			
			auto contents = readCodeFile(canonicalPath.string());
			return ReadCallback::Result{true, contents};
		}
		catch (Exception const& _exception)
		{
			return ReadCallback::Result{false, "Exception in read callback: " + boost::diagnostic_information(_exception)};
		}
		catch (...)
		{
			return ReadCallback::Result{false, "Unknown exception in read callback."};
		}
	};

	try{

		std::string name = NODE_NAME;
		CompilerStack compiler(fileReader);
		compiler.addSource(name, c_node_code);
		compiler.setOptimiserSettings(true, 200);

		bool successful = compiler.compile();
		//cout << "successful=" << successful << endl;
		if(successful){
			vector<string> contracts = compiler.contractNames();

			for (string const& contractName: contracts){
				if(contractName.substr(0, name.length()) == name){
					Json::Value evmData(Json::objectValue);

					evmData["abi"] = compiler.contractABI(contractName);
					evmData["address"] = nodeAddress();
					evmData["gasEstimates"] = compiler.gasEstimates(contractName);
					//cout << "contractName=" << contractName << ",runtimeObject=" << runtimeObject << endl;

					Json::FastWriter fastWriter;
					std::string output = fastWriter.write(evmData);

					return output;
				}
			}
			
		}

	}
	catch (CompilerError const& _exception)
	{
		cerr << "Compiler error:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (InternalCompilerError const& _exception)
	{
		cerr << "Internal compiler error during compilation:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (UnimplementedFeatureError const& _exception)
	{
		cerr << "Unimplemented feature:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (Error const& _error)
	{
		cerr << "Documentation parsing error:" << endl
					 << boost::diagnostic_information(_error);

		return "";
	}
	catch (Exception const& _exception)
	{
		cerr << "Exception during compilation: " << boost::diagnostic_information(_exception) << endl;
		return "";
	}
	catch (...)
	{
		cerr << "Unknown exception during compilation." << endl;
		return "";
	}
	
	return "";
}

std::string compileNode()
{
	//return compileFile(NODE_NAME);
	return compileCode(NODE_NAME, c_node_code);
}

std::string nodeAddress()
{
	return "0x0000000000000000000000000000000000000110";
}

std::string compileCode(std::string name, std::string code)
{
	ReadCallback::Callback fileReader = [](string const& _path)
	{
		try
		{
			auto path = boost::filesystem::path(_path);
			auto canonicalPath = boost::filesystem::canonical(path);
			
			auto contents = readCodeFile(canonicalPath.string());
			return ReadCallback::Result{true, contents};
		}
		catch (Exception const& _exception)
		{
			return ReadCallback::Result{false, "Exception in read callback: " + boost::diagnostic_information(_exception)};
		}
		catch (...)
		{
			return ReadCallback::Result{false, "Unknown exception in read callback."};
		}
	};

	try{

		CompilerStack compiler(fileReader);
		compiler.addSource(name, code);
		compiler.setOptimiserSettings(true, 200);

		bool successful = compiler.compile();
		//cout << "successful=" << successful << endl;
		if(successful){
			vector<string> contracts = compiler.contractNames();

			for (string const& contractName: contracts){
				if(contractName.substr(0, name.length()) == name){
					auto runtimeObject = compiler.runtimeObject(contractName).toHex();
					cout << "contractName=" << contractName << endl;
					return runtimeObject;
				}
			}
			
		}

	}
	catch (CompilerError const& _exception)
	{
		cerr << "Compiler error:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (InternalCompilerError const& _exception)
	{
		cerr << "Internal compiler error during compilation:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (UnimplementedFeatureError const& _exception)
	{
		cerr << "Unimplemented feature:" << endl
			 << boost::diagnostic_information(_exception);
		return "";
	}
	catch (Error const& _error)
	{
		cerr << "Documentation parsing error:" << endl
					 << boost::diagnostic_information(_error);

		return "";
	}
	catch (Exception const& _exception)
	{
		cerr << "Exception during compilation: " << boost::diagnostic_information(_exception) << endl;
		return "";
	}
	catch (...)
	{
		cerr << "Unknown exception during compilation." << endl;
		return "";
	}
	
	return "";
}

std::string compileFile(std::string name)
{
	string code = readCodeFile(name);
	
	if(code.empty())
		return std::string();

	cout << "name=" << name << ",code == c_node_code=" << (code == c_node_code) << endl;
	return compileCode(name, code);
}

