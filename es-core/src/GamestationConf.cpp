#include "GamestationConf.h"
#include <iostream>
#include <fstream>
#include <boost/regex.hpp>
#include "Log.h"
#include <boost/algorithm/string/predicate.hpp>

GamestationConf *GamestationConf::sInstance = NULL;
boost::regex validLine("^(?<key>[^;|#].*?)=(?<val>.*?)$");
boost::regex commentLine("^;(?<key>.*?)=(?<val>.*?)$");

std::string gamestationConfFile = "/userdata/system/gamestation.conf";
std::string gamestationConfFileTmp = "/userdata/system/gamestation.conf.tmp";

GamestationConf::GamestationConf() {
    loadGamestationConf();
}

GamestationConf *GamestationConf::getInstance() {
    if (sInstance == NULL)
        sInstance = new GamestationConf();

    return sInstance;
}

bool GamestationConf::loadGamestationConf() {
    std::string line;
    std::ifstream gamestationConf(gamestationConfFile);
    if (gamestationConf && gamestationConf.is_open()) {
        while (std::getline(gamestationConf, line)) {
            boost::smatch lineInfo;
            if (boost::regex_match(line, lineInfo, validLine)) {
                confMap[std::string(lineInfo["key"])] = std::string(lineInfo["val"]);
            }
        }
        gamestationConf.close();
    } else {
        LOG(LogError) << "Unable to open " << gamestationConfFile;
        return false;
    }
    return true;
}


bool GamestationConf::saveGamestationConf() {
    std::ifstream filein(gamestationConfFile); //File to read from
    if (!filein) {
        LOG(LogError) << "Unable to open for saving :  " << gamestationConfFile << "\n";
        return false;
    }
    /* Read all lines in a vector */
    std::vector<std::string> fileLines;
    std::string line;
    while (std::getline(filein, line)) {
        fileLines.push_back(line);
    }
    filein.close();


    /* Save new value if exists */
    for (std::map<std::string, std::string>::iterator it = confMap.begin(); it != confMap.end(); ++it) {
        std::string key = it->first;
        std::string val = it->second;
        bool lineFound = false;
        for (int i = 0; i < fileLines.size(); i++) {
            std::string currentLine = fileLines[i];

            if (boost::starts_with(currentLine, key+"=") || boost::starts_with(currentLine, ";"+key+"=")){
	      if(val != "" && val != "auto") {
                fileLines[i] = key + "=" + val;
	      } else {
                fileLines[i] = ";" + key + "=" + val;
	      }
	      lineFound = true;
            }
        }
        if(!lineFound) {
	  if(val != "" && val != "auto") {
            fileLines.push_back(key + "=" + val);
	  }
        }
    }
    std::ofstream fileout(gamestationConfFileTmp); //Temporary file
    if (!fileout) {
        LOG(LogError) << "Unable to open for saving :  " << gamestationConfFileTmp << "\n";
        return false;
    }
    for (int i = 0; i < fileLines.size(); i++) {
        fileout << fileLines[i] << "\n";
    }

    fileout.close();


    /* Copy back the tmp to gamestation.conf */
    std::ifstream  src(gamestationConfFileTmp, std::ios::binary);
    std::ofstream  dst(gamestationConfFile,   std::ios::binary);
    dst << src.rdbuf();

    remove(gamestationConfFileTmp.c_str());

    return true;
}

std::string GamestationConf::get(const std::string &name) {
    if (confMap.count(name)) {
        return confMap[name];
    }
    return "";
}
std::string GamestationConf::get(const std::string &name, const std::string &defaut) {
    if (confMap.count(name)) {
        return confMap[name];
    }
    return defaut;
}

void GamestationConf::set(const std::string &name, const std::string &value) {
    confMap[name] = value;
}
