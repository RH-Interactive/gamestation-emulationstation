/*
 * File:   GamestationSystem.cpp
 * Author: digitallumberjack
 * 
 * Created on 29 novembre 2014, 03:15
 */

#include "GamestationSystem.h"
#include <stdlib.h>
#if !defined(WIN32)
#include <sys/statvfs.h>
#endif
#include <sstream>
#include "Settings.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "HttpReq.h"

#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"

#include <stdio.h>
#include <sys/types.h>
#if !defined(WIN32)
#include <ifaddrs.h>
#include <netinet/in.h>
#endif
#include <string.h>
#if !defined(WIN32)
#include <arpa/inet.h>
#endif
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <fstream>

#if defined(WIN32)
#define popen _popen
#define pclose _pclose
#endif

GamestationSystem::GamestationSystem() {
}

GamestationSystem *GamestationSystem::instance = NULL;


GamestationSystem *GamestationSystem::getInstance() {
    if (GamestationSystem::instance == NULL) {
        GamestationSystem::instance = new GamestationSystem();
    }
    return GamestationSystem::instance;
}

unsigned long GamestationSystem::getFreeSpaceGB(std::string mountpoint) {
#if !defined(WIN32)
    struct statvfs fiData;
    const char *fnPath = mountpoint.c_str();
    int free = 0;
    if ((statvfs(fnPath, &fiData)) >= 0) {
        free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
    }
    return free;
#else
    return 0;
#endif
}

std::string GamestationSystem::getFreeSpaceInfo() {
    std::string sharePart = "/userdata/";
    if (sharePart.size() > 0) {
        const char *fnPath = sharePart.c_str();
#if !defined(WIN32)
        struct statvfs fiData;
        if ((statvfs(fnPath, &fiData)) < 0) {
            return "";
        } else {
            unsigned long total = (fiData.f_blocks * (fiData.f_bsize / 1024)) / (1024L * 1024L);
            unsigned long free = (fiData.f_bfree * (fiData.f_bsize / 1024)) / (1024L * 1024L);
            unsigned long used = total - free;
            unsigned long percent = 0;
            std::ostringstream oss;
            if (total != 0){  //for small SD card ;) with share < 1GB
				percent = used * 100 / total;
                oss << used << "GB/" << total << "GB (" << percent << "%)";
			}
			else
			    oss << "N/A";
            return oss.str();
        }
#else
        (void)(fnPath);
        return "TODO";
#endif
    } else {
        return "ERROR";
    }
}

bool GamestationSystem::isFreeSpaceLimit() {
    std::string sharePart = "/userdata/";
    if (sharePart.size() > 0) {
        return getFreeSpaceGB(sharePart) < 2;
    } else {
        return "ERROR";
    }

}

std::string GamestationSystem::getVersion() {
    std::string version = "/usr/share/gamestation/gamestation.version";
    if (version.size() > 0) {
        std::ifstream ifs(version);

        if (ifs.good()) {
            std::string contents;
            std::getline(ifs, contents);
            return contents;
        }
    }
    return "";
}

bool GamestationSystem::needToShowVersionMessage() {
    createLastVersionFileIfNotExisting();
    std::string versionFile = "/userdata/system/update.done";
    if (versionFile.size() > 0) {
        std::ifstream lvifs(versionFile);
        if (lvifs.good()) {
            std::string lastVersion;
            std::getline(lvifs, lastVersion);
            std::string currentVersion = getVersion();
            if (lastVersion == currentVersion) {
                return false;
            }
        }
    }
    return true;
}

bool GamestationSystem::createLastVersionFileIfNotExisting() {
    std::string versionFile = "/userdata/system/update.done";

    FILE *file;
    if (file = fopen(versionFile.c_str(), "r")) {
        fclose(file);
        return true;
    }
    return updateLastVersionFile();
}

bool GamestationSystem::updateLastVersionFile() {
    std::string versionFile = "/userdata/system/update.done";
    std::string currentVersion = getVersion();
    std::ostringstream oss;
    oss << "echo " << currentVersion << " > " << versionFile;
    if (system(oss.str().c_str())) {
        LOG(LogWarning) << "Error executing " << oss.str().c_str();
        return false;
    } else {
        LOG(LogError) << "Last version file updated";
        return true;
    }
}

bool GamestationSystem::setOverscan(bool enable) {

    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "overscan";
    if (enable) {
        oss << " " << "enable";
    } else {
        oss << " " << "disable";
    }
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str())) {
        LOG(LogWarning) << "Error executing " << command;
        return false;
    } else {
        LOG(LogInfo) << "Overscan set to : " << enable;
        return true;
    }

}

bool GamestationSystem::setOverclock(std::string mode) {
    if (mode != "") {
        std::ostringstream oss;
        oss << "/gamestation/scripts/gamestation-overclock.sh set " << mode;
        std::string command = oss.str();
        LOG(LogInfo) << "Launching " << command;
        if (system(command.c_str())) {
            LOG(LogWarning) << "Error executing " << command;
            return false;
        } else {
            LOG(LogInfo) << "Overclocking set to " << mode;
            return true;
        }
    }

    return false;
}


std::pair<std::string,int> GamestationSystem::updateSystem(BusyComponent* ui) {
  std::string updatecommand = "/gamestation/scripts/gamestation-upgrade.sh";
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call update command"),-1);
    }

    FILE *flog = fopen("/userdata/system/logs/gamestation-upgrade.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> GamestationSystem::backupSystem(BusyComponent* ui, std::string device) {
    std::string updatecommand = std::string("/gamestation/scripts/gamestation-sync.sh sync ") + device;
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call sync command"),-1);
    }

    FILE *flog = fopen("/userdata/system/logs/gamestation-sync.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> GamestationSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) {
    std::string updatecommand = std::string("/gamestation/scripts/gamestation-install.sh install ") + device + " " + architecture;
    FILE *pipe = popen(updatecommand.c_str(), "r");
    char line[1024] = "";
    if (pipe == NULL) {
        return std::pair<std::string,int>(std::string("Cannot call install command"),-1);
    }

    FILE *flog = fopen("/userdata/system/logs/gamestation-install.log", "w");
    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
	if(flog != NULL) fprintf(flog, "%s\n", line);
        ui->setText(std::string(line));
    }
    if(flog != NULL) fclose(flog);

    int exitCode = pclose(pipe);
    return std::pair<std::string,int>(std::string(line), exitCode);
}

std::pair<std::string,int> GamestationSystem::scrape(BusyComponent* ui) {
  std::string scrapecommand = std::string("/gamestation/scripts/gamestation-scraper.sh");
  FILE *pipe = popen(scrapecommand.c_str(), "r");
  char line[1024] = "";
  if (pipe == NULL) {
    return std::pair<std::string,int>(std::string("Cannot call scrape command"),-1);
  }
  
  FILE *flog = fopen("/userdata/system/logs/gamestation-scrape.log", "w");
  while (fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    if(flog != NULL) fprintf(flog, "%s\n", line);

    if(boost::starts_with(line, "GAME: ")) {
      ui->setText(std::string(line));
    }
  }
  if(flog != NULL) fclose(flog);
  
  int exitCode = pclose(pipe);
  return std::pair<std::string,int>(std::string(line), exitCode);
}

bool GamestationSystem::ping() {
    std::string updateserver = "gamestation-linux.xorhub.com";
    std::string s("timeout -t 1 fping -c 1 -t 1000 " + updateserver);
    int exitcode = system(s.c_str());
    return exitcode == 0;
}

bool GamestationSystem::canUpdate(std::vector<std::string>& output) {
    int res;
    int exitCode;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "canupdate";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;

    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
      return false;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        output.push_back(std::string(line));
    }
    exitCode = pclose(pipe);
    res = WEXITSTATUS(exitCode);

    if (res == 0) {
        LOG(LogInfo) << "Can update ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot update ";
        return false;
    }
}

bool GamestationSystem::launchKodi(Window *window) {

    LOG(LogInfo) << "Attempting to launch kodi...";


    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    std::string commandline = InputManager::getInstance()->configureEmulators();
    std::string command = "configgen -system kodi -rom '' " + commandline;

    window->deinit();
    int exitCode = system(command.c_str());
#if !defined(WIN32)
    // WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
    // https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
    if (WIFEXITED(exitCode)) {
        exitCode = WEXITSTATUS(exitCode);
    }
#endif

    window->init();
    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    window->normalizeNextUpdate();

    // handle end of kodi
    switch (exitCode) {
        case 10: // reboot code
            reboot();
            return true;
            break;
        case 11: // shutdown code
            shutdown();
            return true;
            break;
    }

    return exitCode == 0;

}

bool GamestationSystem::launchFileManager(Window *window) {
    LOG(LogInfo) << "Attempting to launch filemanager...";

    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    std::string command = "/gamestation/scripts/filemanagerlauncher.sh";

    window->deinit();

    int exitCode = system(command.c_str());
#if !defined(WIN32)
    if (WIFEXITED(exitCode)) {
        exitCode = WEXITSTATUS(exitCode);
    }
#endif

    window->init();
    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    window->normalizeNextUpdate();

    return exitCode == 0;
}

bool GamestationSystem::enableWifi(std::string ssid, std::string key) {
    std::ostringstream oss;
    boost::replace_all(ssid, "\"", "\\\"");
    boost::replace_all(key, "\"", "\\\"");
    oss << "/gamestation/scripts/gamestation-config.sh" << " "
    << "wifi" << " "
    << "enable" << " \""
    << ssid << "\" \"" << key << "\"";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str()) == 0) {
        LOG(LogInfo) << "Wifi enabled ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot enable wifi ";
        return false;
    }
}

bool GamestationSystem::disableWifi() {
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " "
    << "wifi" << " "
    << "disable";
    std::string command = oss.str();
    LOG(LogInfo) << "Launching " << command;
    if (system(command.c_str()) == 0) {
        LOG(LogInfo) << "Wifi disabled ";
        return true;
    } else {
        LOG(LogInfo) << "Cannot disable wifi ";
        return false;
    }
}


bool GamestationSystem::halt(bool reboot, bool fast) {
    SDL_Event *quit = new SDL_Event();
    if (fast)
        if (reboot)
            quit->type = SDL_FAST_QUIT | SDL_RB_REBOOT;
        else
            quit->type = SDL_FAST_QUIT | SDL_RB_SHUTDOWN;
    else
        if (reboot)
            quit->type = SDL_QUIT | SDL_RB_REBOOT;
        else
            quit->type = SDL_QUIT | SDL_RB_SHUTDOWN;
    SDL_PushEvent(quit);
    return 0;
}

bool GamestationSystem::reboot() {
    return halt(true, false);
}

bool GamestationSystem::fastReboot() {
    return halt(true, true);
}

bool GamestationSystem::shutdown() {
    return halt(false, false);
}

bool GamestationSystem::fastShutdown() {
    return halt(false, true);
}


std::string GamestationSystem::getIpAdress() {
#if !defined(WIN32)
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;

    std::string result = "NOT CONNECTED";
    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
            if (std::string(ifa->ifa_name).find("eth") != std::string::npos ||
                std::string(ifa->ifa_name).find("wlan") != std::string::npos) {
                result = std::string(addressBuffer);
            }
        }
    }
    // Seeking for ipv6 if no IPV4
    if (result.compare("NOT CONNECTED") == 0) {
        for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
                // is a valid IP6 Address
                tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
                char addressBuffer[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
                if (std::string(ifa->ifa_name).find("eth") != std::string::npos ||
                    std::string(ifa->ifa_name).find("wlan") != std::string::npos) {
                    return std::string(addressBuffer);
                }
            }
        }
    }
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    return result;
#else
    return std::string();
#endif
}

bool GamestationSystem::scanNewBluetooth() {
    std::vector<std::string> *res = new std::vector<std::string>();
    std::ostringstream oss;
    oss << "/gamestation/scripts/bluetooth/pair-device";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return false;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res->push_back(std::string(line));
    }

    int exitCode = pclose(pipe);
    return exitCode == 0;
}

std::vector<std::string> GamestationSystem::getAvailableStorageDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "storage list";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> GamestationSystem::getVideoModes() {
  std::vector<std::string> res;
  std::ostringstream oss;
  oss << "gamestation-resolution listModes";
  FILE *pipe = popen(oss.str().c_str(), "r");
  char line[1024];
  
  if (pipe == NULL) {
    return res;
  }
  
  while (fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    res.push_back(std::string(line));
  }
  pclose(pipe);

  return res;
}

std::vector<std::string> GamestationSystem::getAvailableBackupDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-sync.sh list";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> GamestationSystem::getAvailableInstallDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-install.sh listDisks";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> GamestationSystem::getAvailableInstallArchitectures() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-install.sh listArchs";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> GamestationSystem::getAvailableOverclocking() {
  std::vector<std::string> res;
  std::ostringstream oss;
  oss << "/gamestation/scripts/gamestation-overclock.sh list";
  FILE *pipe = popen(oss.str().c_str(), "r");
  char line[1024];
  
  if (pipe == NULL) {
    return res;
  }
  
  while (fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    res.push_back(std::string(line));
  }
  pclose(pipe);
  
  return res;
}

std::vector<std::string> GamestationSystem::getSystemInformations() {
    std::vector<std::string> res;
    FILE *pipe = popen("/gamestation/scripts/gamestation-info.sh", "r");
    char line[1024];

    if (pipe == NULL) {
      return res;
    }

    while(fgets(line, 1024, pipe)) {
      strtok(line, "\n");
      res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<BiosSystem> GamestationSystem::getBiosInformations() {
  std::vector<BiosSystem> res;
  BiosSystem current;
  bool isCurrent = false;

  FILE *pipe = popen("/gamestation/scripts/gamestation-systems.py", "r");
  char line[1024];

  if (pipe == NULL) {
    return res;
  }

  while(fgets(line, 1024, pipe)) {
    strtok(line, "\n");
    if(boost::starts_with(line, "> ")) {
      if(isCurrent) {
	res.push_back(current);
      }
      isCurrent = true;
      current.name = std::string(std::string(line).substr(2));
      current.bios.clear();
    } else {
      BiosFile biosFile;
      std::vector<std::string> tokens;
      boost::split( tokens, line, boost::is_any_of(" "));

      if(tokens.size() >= 3) {
	biosFile.status = tokens.at(0);
	biosFile.md5    = tokens.at(1);

	// concatenat the ending words
	std::string vname = "";
	for(unsigned int i=2; i<tokens.size(); i++) {
	  if(i > 2) vname += " ";
	  vname += tokens.at(i);
	}
	biosFile.path = vname;

	current.bios.push_back(biosFile);
      }
    }
  }
  if(isCurrent) {
    res.push_back(current);
  }
  pclose(pipe);
  return res;
}

bool GamestationSystem::generateSupportFile() {
#if !defined(WIN32)
  std::string cmd = "/gamestation/scripts/gamestation-support.sh";
  int exitcode = system(cmd.c_str());
  if (WIFEXITED(exitcode)) {
    exitcode = WEXITSTATUS(exitcode);
  }
  return exitcode == 0;
#else
    return false;
#endif
}

std::string GamestationSystem::getCurrentStorage() {

    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "storage current";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return "INTERNAL";
}

bool GamestationSystem::setStorage(std::string selected) {
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "storage" << " " << selected;
    int exitcode = system(oss.str().c_str());
    return exitcode == 0;
}

bool GamestationSystem::forgetBluetoothControllers() {
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "forgetBT";
    int exitcode = system(oss.str().c_str());
    return exitcode == 0;
}

std::string GamestationSystem::getRootPassword() {

    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "getRootPassword";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if(fgets(line, 1024, pipe)){
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return oss.str().c_str();
}

std::vector<std::string> GamestationSystem::getAvailableAudioOutputDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "lsaudio";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::vector<std::string> GamestationSystem::getAvailableVideoOutputDevices() {

    std::vector<std::string> res;
    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "lsoutputs";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return res;
    }

    while (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        res.push_back(std::string(line));
    }
    pclose(pipe);

    return res;
}

std::string GamestationSystem::getCurrentAudioOutputDevice() {

    std::ostringstream oss;
    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "getaudio";
    FILE *pipe = popen(oss.str().c_str(), "r");
    char line[1024];

    if (pipe == NULL) {
        return "";
    }

    if (fgets(line, 1024, pipe)) {
        strtok(line, "\n");
        pclose(pipe);
        return std::string(line);
    }
    return "INTERNAL";
}

bool GamestationSystem::setAudioOutputDevice(std::string selected) {
    std::ostringstream oss;

    AudioManager::getInstance()->deinit();
    VolumeControl::getInstance()->deinit();

    oss << "/gamestation/scripts/gamestation-config.sh" << " " << "audio" << " '" << selected << "'";
    int exitcode = system(oss.str().c_str());

    VolumeControl::getInstance()->init();
    AudioManager::getInstance()->resumeMusic();
    AudioManager::getInstance()->playCheckSound();

    return exitcode == 0;
}
