//
// Created by matthieu on 12/09/15.
//

#ifndef EMULATIONSTATION_ALL_GAMESTATIONCONF_H
#define EMULATIONSTATION_ALL_GAMESTATIONCONF_H


#include <string>
#include <map>

class GamestationConf {

public:
    GamestationConf();

    bool loadGamestationConf();

    bool saveGamestationConf();

    std::string get(const std::string &name);
    std::string get(const std::string &name, const std::string &defaut);

    void set(const std::string &name, const std::string &value);

    static GamestationConf *sInstance;

    static GamestationConf *getInstance();
private:
    std::map<std::string, std::string> confMap;

};


#endif //EMULATIONSTATION_ALL_GAMESTATIONCONF_H
