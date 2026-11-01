// Copyright (c) 2026 Dalsin Limited. MIT licensed.
#include <OpenLoco/Platform/Platform.h>
#include <OpenLoco/Core/Exception.hpp>
#include <SDL3/SDL.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
namespace OpenLoco::Platform
{
    // BEGIN instance-lock (probes/instance_lock builds this part on its own)
    /* One OpenLoco at a time. The running game holds OpenLoco.lock open with
     * MODE_NEWFILE (an exclusive lock) and writes its process address and name
     * to OpenLoco.owner. AmigaOS frees nothing a crashed program held: a game
     * that crashed and was suspended, or was removed, keeps that lock until
     * the next reboot. So when the lock is taken, OpenLoco.owner says by whom;
     * an owner that is gone, or waits for no signal at all (a task held after
     * a crash), can never give it back, and the game starts anyway. */
    static BPTR instanceFile;
    static std::string instanceOwnerPath;
    static void releaseInstance() {
        if(instanceFile) { Close(instanceFile);instanceFile=0; }
        if(!instanceOwnerPath.empty()) { DeleteFile(instanceOwnerPath.c_str());instanceOwnerPath.clear(); }
    }
    static bool writeInstanceOwner(const std::string& path) {
        struct Task* self=FindTask(nullptr);
        char line[128];
        std::snprintf(line,sizeof line,"%08lx %.100s\n",(unsigned long)self,
                      self->tc_Node.ln_Name?self->tc_Node.ln_Name:"");
        BPTR f=Open(path.c_str(),MODE_NEWFILE);
        if(!f) return false;
        const LONG n=(LONG)std::strlen(line);
        const bool ok=Write(f,line,n)==n;
        Close(f);
        return ok;
    }
    /* Whether the process recorded in OWNER can still run and release the lock. */
    static bool instanceOwnerAlive(const std::string& path) {
        char line[128]={0};
        BPTR f=Open(path.c_str(),MODE_OLDFILE);
        if(!f) return true;                 // no record: assume it is running
        const LONG n=Read(f,line,sizeof line-1);
        Close(f);
        if(n<=0) return true;
        line[n]=0;
        char* end=nullptr;
        const unsigned long address=std::strtoul(line,&end,16);
        if(end==line || *end!=' ') return true;
        char* name=end+1;
        name[std::strcspn(name,"\n")]=0;
        if(address==(unsigned long)FindTask(nullptr)) return true;
        bool found=false, waitsForNothing=false;
        char found_name[104]={0};
        struct ExecBase* exec=SysBase;
        struct List* lists[2]={&exec->TaskReady,&exec->TaskWait};
        Disable();                          // the task lists change in interrupts
        for(int i=0;i<2 && !found;++i)
            for(struct Node* node=lists[i]->lh_Head;node->ln_Succ;node=node->ln_Succ)
                if((unsigned long)node==address) {
                    struct Task* task=(struct Task*)node;
                    found=true;
                    waitsForNothing=i==1 && task->tc_SigWait==0;
                    if(node->ln_Name) std::strncpy(found_name,node->ln_Name,sizeof found_name-1);
                    break;
                }
        Enable();
        return found && !waitsForNothing && std::strcmp(found_name,name)==0;
    }
    static bool lockInstanceFiles(const std::string& lockPath,const std::string& ownerPath) {
        if(instanceFile || !instanceOwnerPath.empty()) return true;
        instanceFile=Open(lockPath.c_str(),MODE_NEWFILE);
        if(!instanceFile) {
            if(IoErr()!=ERROR_OBJECT_IN_USE || instanceOwnerAlive(ownerPath)) return false;
            // the holder is dead: its lock stays until reboot, the owner record is ours
        }
        if(!writeInstanceOwner(ownerPath) && !instanceFile) return false;
        instanceOwnerPath=ownerPath;
        std::atexit(releaseInstance);
        return true;
    }
    // END instance-lock
    void initialise() {}
    uint32_t getTime() { return static_cast<uint32_t>(SDL_GetTicks()); }
    std::string getEnvironmentVariable(const std::string& name) {
        const char* v=std::getenv(name.c_str());
        if(v) return v;
        char buffer[4096];
        return GetVar(name.c_str(),buffer,sizeof buffer,0)>0?std::string(buffer):std::string();
    }
    static std::string programDirectory() {
        char path[4096];
        if(!NameFromLock(GetProgramDir(),path,sizeof path))
            throw Exception::RuntimeError("Cannot determine Amiga program directory");
        std::string result(path);
        if(!result.empty() && result.back()!=':' && result.back()!='/') result+='/';
        return result;
    }
    fs::path getCurrentExecutablePath() {
        char name[1024];
        if(!GetProgramName(name,sizeof name))
            throw Exception::RuntimeError("Cannot determine Amiga program name");
        return fs::path(programDirectory()+FilePart(name));
    }
    fs::path getDataDirectory() {
        auto path=getEnvironmentVariable("OPENLOCO_DATA_DIR");
        return fs::path(path.empty()?programDirectory()+"data":path);
    }
    fs::path getUserDirectory() {
        auto path=getEnvironmentVariable("OPENLOCO_USER_DIR");
        return fs::path(path.empty()?programDirectory()+"User":path);
    }
    std::vector<fs::path> getDrives() { return {}; }
    std::vector<fs::path> getLocoInstallSearchPaths() {
        const auto path=getEnvironmentVariable("LOCOMOTION_DATA_DIR");
        return path.empty()?std::vector<fs::path>{}:std::vector<fs::path>{fs::path(path)};
    }
    fs::path promptDirectory(const std::string&, void*) {
        std::string input;std::cout<<"Locomotion data folder: ";std::getline(std::cin,input);
        return fs::path(input);
    }
    bool isRunningInWine() { return false; }
    bool isStdOutRedirected() { return !IsInteractive(Output()); }
    bool hasTerminalVT100Support() { return false; }
    bool enableVT100TerminalMode() { return false; }
    std::vector<std::string> getCmdLineVector(int argc,const char** argv) {
        return std::vector<std::string>(argv,argv+argc);
    }
    bool lockSingleInstance() {
        if(instanceFile || !instanceOwnerPath.empty()) return true;
        const auto user=getUserDirectory();
        fs::create_directories(user);
        return lockInstanceFiles((user/"OpenLoco.lock").string(),(user/"OpenLoco.owner").string());
    }
}
