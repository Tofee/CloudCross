/*
    CloudCross: Opensource program for syncronization of local files and folders with clouds

    Copyright (C) 2016  Vladimir Kamensky
    Copyright (C) 2016  Master Soft LLC.
    All rights reserved.


  BSD License

  Redistribution and use in source and binary forms, with or without modification, are
  permitted provided that the following conditions are met:

  - Redistributions of source code must retain the above copyright notice, this list of
    conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice, this list
    of conditions and the following disclaimer in the documentation and/or other
    materials provided with the distribution.
  - Neither the name of the "Vladimir Kamensky" or "Master Soft LLC." nor the names of
    its contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY E
  XPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES O
  F MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SH
  ALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENT
  AL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROC
  UREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS I
  NTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRI
  CT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF T
  HE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "msyandexdisk.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#define YADISK_MAX_FILESIZE  8000000
//  150000000

MSYandexDisk::MSYandexDisk() :
    MSCloudProvider()
{
    this->providerName=     "YandexDisk";
    this->tokenFileName=    ".yadisk";
    this->stateFileName=    ".yadisk_state";
    this->trashFileName=    ".trash_yadisk";

}


//=======================================================================================

bool MSYandexDisk::auth(){


    connect(this,SIGNAL(oAuthCodeRecived(QString,MSCloudProvider*)),this,SLOT(onAuthFinished(QString, MSCloudProvider*)));
    connect(this,SIGNAL(oAuthError(QString,MSCloudProvider*)),this,SLOT(onAuthFinished(QString, MSCloudProvider*)));


    this->startListener(1973);

    qStdOut()<<"-------------------------------------"<<endl;
    qStdOut()<< tr("Please go to this URL and confirm application credentials\n")<<endl;


    qStdOut() << "https://oauth.yandex.ru/authorize?force_confirm=yes&response_type=code&client_id=ba0517299fbc4e6db5ec7c040baccca7&state=" <<this->generateRandom(20)<<endl;



    QEventLoop loop;
    connect(this, SIGNAL(providerAuthComplete()), &loop, SLOT(quit()));
    loop.exec();


    return true;

}


//=======================================================================================

bool MSYandexDisk::onAuthFinished(QString html, MSCloudProvider *provider){

Q_UNUSED(provider)

    MSRequest* req=new MSRequest(this->proxyServer);

    req->setRequestUrl("https://oauth.yandex.ru/token");
    req->setMethod("post");

    req->addQueryItem("client_id",          "ba0517299fbc4e6db5ec7c040baccca7");
    req->addQueryItem("client_secret",      "aed48ba99efb45a78f5138f63059bfc5");
    req->addQueryItem("code",               html.trimmed());
    req->addQueryItem("grant_type",         "authorization_code");
    //req->addQueryItem("redirect_uri",       "http://127.0.0.1:1973");

    req->exec();


    if(!req->replyOK()){
        delete(req);
        req->printReplyError();
        this->providerAuthStatus=false;
        emit providerAuthComplete();
        return false;
    }

    QString content= req->replyText;

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString v=job["access_token"].toString();

    if(v!=""){

        this->token=v;
        qStdOut() << "Token was succesfully accepted and saved. To start working with the program run ccross without any options for start full synchronize."<<endl;
        this->providerAuthStatus=true;
        emit providerAuthComplete();
        return true;
    }
    else{
        this->providerAuthStatus=false;
        emit providerAuthComplete();
        return false;
    }
}

//=======================================================================================

void MSYandexDisk::saveTokenFile(QString path){

    QFile key(path+"/"+this->tokenFileName);
    key.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outk(&key);
    outk << "{\"access_token\" : \""+this->token+"\"}";
    key.close();

}


//=======================================================================================

bool MSYandexDisk::loadTokenFile(QString path){

    QFile key(path+"/"+this->tokenFileName);

    if(!key.open(QIODevice::ReadOnly))
    {
        qStdOut() << "Access key missing or corrupt. Start CloudCross with -a option for obtained private key."  <<endl;
        return false;
    }

    QTextStream instream(&key);
    QString line;
    while(!instream.atEnd()){

        line+=instream.readLine();
    }

    QJsonDocument json = QJsonDocument::fromJson(line.toUtf8());
    QJsonObject job = json.object();
    QString v=job["access_token"].toString();

    this->token=v;

    key.close();
    return true;
}


//=======================================================================================

void MSYandexDisk::loadStateFile(){

    QFile key(this->workPath+"/"+this->stateFileName);

    if(!key.open(QIODevice::ReadOnly))
    {
        qStdOut() << "Previous state file not found. Start in stateless mode."<<endl ;
        return ;
    }

    QTextStream instream(&key);
    QString line;
    while(!instream.atEnd()){

        line+=instream.readLine();
    }

    QJsonDocument json = QJsonDocument::fromJson(line.toUtf8());
    QJsonObject job = json.object();

    this->lastSyncTime=QJsonValue(job["last_sync"].toObject()["sec"]).toVariant().toULongLong();

    key.close();
    return ;
}

//=======================================================================================

void MSYandexDisk::saveStateFile(){


    QJsonDocument state;
    QJsonObject jso;
    jso.insert("change_stamp",QString("0"));

    QJsonObject jts;
    jts.insert("nsec",QString("0"));
    jts.insert("sec",QString::number(QDateTime( QDateTime::currentDateTime()).toMSecsSinceEpoch()));

    jso.insert("last_sync",jts);
    state.setObject(jso);

    QFile key(this->workPath+"/"+this->stateFileName);
    key.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outk(&key);
    outk << state.toJson();
    key.close();

}



//=======================================================================================

bool MSYandexDisk::refreshToken(){

    this->access_token=this->token;
    return true;
}

//=======================================================================================

bool MSYandexDisk::createHashFromRemote(){

  //<<<<<<<<  this->readRemote();

    MSRequest* req=new MSRequest(this->proxyServer);

    req->setRequestUrl("https://api.dropboxapi.com/2/files/list_folder");
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));




    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }




    QString list=req->readReplyText();




    delete(req);
    return true;
}


//=======================================================================================

bool MSYandexDisk::readRemote(QString currentPath){ //QString parentId,QString currentPath


    MSRequest* req=new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net:443/v1/disk/resources");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));

    req->addQueryItem("limit",          "1000000");
    req->addQueryItem("offset",          "0");
    req->addQueryItem("path",          currentPath);


    req->exec();

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }



    QString list=req->readReplyText();

    delete(req);

    QJsonDocument json = QJsonDocument::fromJson(list.toUtf8());
    QJsonObject job = json.object();
    QJsonArray entries= job["_embedded"].toObject()["items"].toArray();

    QFileInfo fi=QFileInfo(entries[0].toObject()["path"].toString()) ;

    int hasMore=entries.size();


    do{


        for(int i=0;i<entries.size();i++){

                    MSFSObject fsObject;

                    QJsonObject o=entries[i].toObject();

                    QString yPath=o["path"].toString();
                    yPath.replace("disk:/","/");

                    fsObject.path = QFileInfo(yPath).path()+"/";

                    if(fsObject.path == "//"){
                       fsObject.path ="/";
                    }


                    // test for non first-level files


                    fsObject.remote.fileSize=  o["size"].toInt();
                    fsObject.remote.data=o;
                    fsObject.remote.exist=true;
                    fsObject.isDocFormat=false;

                    fsObject.state=MSFSObject::ObjectState::NewRemote;

                    if(this->isFile(o)){

                        fsObject.fileName=o["name"].toString();
                        fsObject.remote.objectType=MSRemoteFSObject::Type::file;
                        fsObject.remote.modifiedDate=this->toMilliseconds(o["modified"].toString(),true);
                        fsObject.remote.md5Hash=o["md5"].toString();

                     }

                     if(this->isFolder(o)){

                         fsObject.fileName=o["name"].toString();
                         fsObject.remote.objectType=MSRemoteFSObject::Type::folder;
                         fsObject.remote.modifiedDate=this->toMilliseconds(o["modified"].toString(),true);

                      }

                      if(! this->filterServiceFileNames(yPath)){// skip service files and dirs

                          continue;
                      }


                      if(this->getFlag("useInclude") && this->includeList != ""){//  --use-include

                          if( this->filterIncludeFileNames(yPath)){

                              continue;
                          }
                      }
                      else{// use exclude by default

                      if(this->excludeList != ""){
                      if(! this->filterExcludeFileNames(yPath)){

                          continue;
                      }
                      }
                      }


                      if(this->isFolder(o)){
                          this->readRemote(yPath);
                      }

                      this->syncFileList.insert(yPath, fsObject);

        }


            MSRequest* req=new MSRequest(this->proxyServer);

            req->setRequestUrl("https://cloud-api.yandex.net:443/v1/disk/resources");
            req->setMethod("get");

            req->addHeader("Authorization",                     "OAuth "+this->access_token);
            req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));

            req->addQueryItem("limit",          "1000000");
            req->addQueryItem("offset",          QString::number(entries.size()));
            req->addQueryItem("path",          currentPath);

            req->exec();

            if(!req->replyOK()){
                req->printReplyError();
                delete(req);
                return false;
            }



            QString list=req->readReplyText();

            delete(req);

            json = QJsonDocument::fromJson(list.toUtf8());
            job = json.object();
            entries= job["_embedded"].toObject()["items"].toArray();

            hasMore=entries.size();




    }while(hasMore > 0);

    return true;

}



//=======================================================================================


bool MSYandexDisk::isFile(QJsonValue remoteObject){
    if(remoteObject.toObject()["type"].toString()=="file"){
        return true;
    }
    return false;
}

//=======================================================================================

bool MSYandexDisk::isFolder(QJsonValue remoteObject){
    if(remoteObject.toObject()["type"].toString()=="dir"){
        return true;
    }
    return false;
}

//=======================================================================================

bool MSYandexDisk::createSyncFileList(){

    if(this->getFlag("useInclude")){
        QFile key(this->workPath+"/.include");

        if(key.open(QIODevice::ReadOnly)){

            QTextStream instream(&key);
            QString line;
            while(!instream.atEnd()){

                line=instream.readLine();
                if(line.isEmpty()){
                    continue;
                }
                if(instream.pos() == 9 && line == "wildcard"){
                    this->options.insert("filter-type", "wildcard");
                    continue;
                }
                else if(instream.pos() == 7 && line == "regexp"){
                    this->options.insert("filter-type", "regexp");
                    continue;
                }
                this->includeList=this->includeList+line+"|";
            }
            this->includeList=this->includeList.left(this->includeList.size()-1);

            QRegExp regex2(this->includeList);
            if(this->getOption("filter-type") == "regexp")
                regex2.setPatternSyntax(QRegExp::RegExp);
            else
                regex2.setPatternSyntax(QRegExp::Wildcard);
            if(!regex2.isValid()){
                qStdOut()<<"Include filelist contains errors. Program will be terminated.";
                return false;
            }
        }
    }
    else{
        QFile key(this->workPath+"/.exclude");

        if(key.open(QIODevice::ReadOnly)){

            QTextStream instream(&key);
            QString line;
            while(!instream.atEnd()){

                line=instream.readLine();
                if(instream.pos() == 9 && line == "wildcard"){
                    this->options.insert("filter-type", "wildcard");
                    continue;
                }
                else if(instream.pos() == 7 && line == "regexp"){
                    this->options.insert("filter-type", "regexp");
                    continue;
                }
                if(line.isEmpty()){
                    continue;
                }
                this->excludeList=this->excludeList+line+"|";
            }
            this->excludeList=this->excludeList.left(this->excludeList.size()-1);

            QRegExp regex2(this->excludeList);
            if(this->getOption("filter-type") == "regexp")
                regex2.setPatternSyntax(QRegExp::RegExp);
            else
                regex2.setPatternSyntax(QRegExp::Wildcard);
            if(!regex2.isValid()){
                qStdOut()<<"Exclude filelist contains errors. Program will be terminated.";
                return false;
            }
        }
    }

    qStdOut()<< "Reading remote files"<<endl;


    //this->createHashFromRemote();

    // begin create



    if(!this->readRemote("/")){// top level files and folders
        qStdOut()<<"Error occured on reading remote files" <<endl;
        return false;

    }

    qStdOut()<<"Reading local files and folders" <<endl;

    if(!this->readLocal(this->workPath)){
        qStdOut()<<"Error occured on local files and folders" <<endl;
        return false;

    }

//this->remote_file_get(&(this->syncFileList.values()[0]));
//this->remote_file_insert(&(this->syncFileList.values()[0]));
//this->remote_file_update(&(this->syncFileList.values()[0]));
// this->remote_file_makeFolder(&(this->syncFileList.values()[0]));
//    this->remote_file_trash(&(this->syncFileList.values()[0]));
// this->remote_createDirectory((this->syncFileList.values()[0].path+this->syncFileList.values()[0].fileName));


    this->doSync();

    return true;
}

//=======================================================================================


bool MSYandexDisk::readLocal(QString path){



    QDir dir(path);
    QDir::Filters entryInfoList_flags=QDir::Files|QDir::Dirs |QDir::NoDotAndDotDot;

    if(! this->getFlag("noHidden")){// if show hidden
        entryInfoList_flags= entryInfoList_flags | QDir::System | QDir::Hidden;
    }

        QFileInfoList files = dir.entryInfoList(entryInfoList_flags);

        foreach(const QFileInfo &fi, files){

            QString Path = fi.absoluteFilePath();
            QString relPath=fi.absoluteFilePath().replace(this->workPath,"");

            if(! this->filterServiceFileNames(relPath)){// skip service files and dirs
                continue;
            }


            if(fi.isDir()){

                readLocal(Path);
            }

            if(this->getFlag("useInclude") && this->includeList != ""){//  --use-include

            if( this->filterIncludeFileNames(relPath)){

                continue;
            }
            }
            else{// use exclude by default

            if(this->excludeList != ""){
                if(! this->filterExcludeFileNames(relPath)){

                continue;
                }
            }
            }



            QHash<QString,MSFSObject>::iterator i=this->syncFileList.find(relPath);



            if(i!=this->syncFileList.end()){// if object exists in Yandex Disk

                MSFSObject* fsObject = &(i.value());


                fsObject->local.fileSize=  fi.size();
                fsObject->local.md5Hash= this->fileChecksum(Path,QCryptographicHash::Md5);
                fsObject->local.exist=true;
                fsObject->getLocalMimeType(this->workPath);

                if(fi.isDir()){
                    fsObject->local.objectType=MSLocalFSObject::Type::folder;
                    fsObject->local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);
                }
                else{

                    fsObject->local.objectType=MSLocalFSObject::Type::file;
                    fsObject->local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);

                }


                fsObject->isDocFormat=false;


                fsObject->state=this->filelist_defineObjectState(fsObject->local,fsObject->remote);

            }
            else{

                MSFSObject fsObject;

                fsObject.state=MSFSObject::ObjectState::NewLocal;

                if(relPath.lastIndexOf("/")==0){
                    fsObject.path="/";
                }
                else{
                    fsObject.path=QString(relPath).left(relPath.lastIndexOf("/"))+"/";
                }

                fsObject.fileName=fi.fileName();
                fsObject.getLocalMimeType(this->workPath);

                fsObject.local.fileSize=  fi.size();
                fsObject.local.md5Hash= this->fileChecksum(Path,QCryptographicHash::Md5);
                fsObject.local.exist=true;

                if(fi.isDir()){
                    fsObject.local.objectType=MSLocalFSObject::Type::folder;
                    fsObject.local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);
                }
                else{

                    fsObject.local.objectType=MSLocalFSObject::Type::file;
                    fsObject.local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);

                }

                fsObject.state=this->filelist_defineObjectState(fsObject.local,fsObject.remote);

                fsObject.isDocFormat=false;


                this->syncFileList.insert(relPath,fsObject);

            }

        }

        return true;
}



//=======================================================================================


MSFSObject::ObjectState MSYandexDisk::filelist_defineObjectState(MSLocalFSObject local, MSRemoteFSObject remote){


    if((local.exist)&&(remote.exist)){ //exists both files

        if(local.md5Hash==remote.md5Hash){


                return MSFSObject::ObjectState::Sync;

        }
        else{

            // compare last modified date for local and remote
            if(local.modifiedDate==remote.modifiedDate){

               // return MSFSObject::ObjectState::Sync;

                if(this->strategy==MSCloudProvider::SyncStrategy::PreferLocal){
                    return MSFSObject::ObjectState::ChangedLocal;
                }
                else{
                    return MSFSObject::ObjectState::ChangedRemote;
                }

            }
            else{

                if(local.modifiedDate > remote.modifiedDate){
                    return MSFSObject::ObjectState::ChangedLocal;
                }
                else{
                    return MSFSObject::ObjectState::ChangedRemote;
                }

            }
        }


    }


    if((local.exist)&&(!remote.exist)){ //exist only local file

        if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){
            return  MSFSObject::ObjectState::NewLocal;
        }
        else{
            return  MSFSObject::ObjectState::DeleteRemote;
        }
    }


    if((!local.exist)&&(remote.exist)){ //exist only remote file

        if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){
            return  MSFSObject::ObjectState::DeleteLocal;
        }
        else{
            return  MSFSObject::ObjectState::NewRemote;
        }
    }

    return  MSFSObject::ObjectState::ErrorState;
}

//=======================================================================================


void MSYandexDisk::doSync(){

    QHash<QString,MSFSObject>::iterator lf;

    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

        // create new folder structure on remote

        qStdOut()<<"Checking folder structure on remote" <<endl;

        QHash<QString,MSFSObject> localFolders=this->filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type::folder);
        localFolders=this->filelist_getFSObjectsByState(localFolders,MSFSObject::ObjectState::NewLocal);

        lf=localFolders.begin();

        while(lf != localFolders.end()){

            this->remote_createDirectory(lf.key());

            lf++;
        }
    }
    else{

        // create new folder structure on local

        qStdOut()<<"Checking folder structure on local" <<endl;

        QHash<QString,MSFSObject> remoteFolders=this->filelist_getFSObjectsByTypeRemote(MSRemoteFSObject::Type::folder);
        remoteFolders=this->filelist_getFSObjectsByState(remoteFolders,MSFSObject::ObjectState::NewRemote);

        lf=remoteFolders.begin();

        while(lf != remoteFolders.end()){

            this->local_createDirectory(this->workPath+lf.key());

            lf++;
        }

        // trash local folder
        QHash<QString,MSFSObject> trashFolders=this->filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type::folder);
        trashFolders=this->filelist_getFSObjectsByState(trashFolders,MSFSObject::ObjectState::DeleteRemote);

        lf=trashFolders.begin();

        while(lf != trashFolders.end()){


            this->local_removeFolder(lf.key());

            lf++;
        }

    }




    // FORCING UPLOAD OR DOWNLOAD FILES AND FOLDERS
    if(this->getFlag("force")){

        if(this->getOption("force")=="download"){

            qStdOut()<<"Start downloading in force mode" <<endl;

            lf=this->syncFileList.begin();

            for(;lf != this->syncFileList.end();lf++){

                MSFSObject obj=lf.value();

                if((obj.state == MSFSObject::ObjectState::Sync)||
                   (obj.state == MSFSObject::ObjectState::NewRemote)||
                   (obj.state == MSFSObject::ObjectState::DeleteLocal)||
                   (obj.state == MSFSObject::ObjectState::ChangedLocal)||
                   (obj.state == MSFSObject::ObjectState::ChangedRemote) ){

                    if(obj.remote.objectType == MSRemoteFSObject::Type::file){

                        qStdOut()<< obj.path<<obj.fileName <<" Forced downloading." <<endl;

                        this->remote_file_get(&obj);
                    }
                }

            }

        }
        else{
            if(this->getOption("force")=="upload"){

                qStdOut()<<"Start uploading in force mode" <<endl;

                lf=this->syncFileList.begin();

                for(;lf != this->syncFileList.end();lf++){

                    MSFSObject obj=lf.value();

                    if((obj.state == MSFSObject::ObjectState::Sync)||
                       (obj.state == MSFSObject::ObjectState::NewLocal)||
                       (obj.state == MSFSObject::ObjectState::DeleteRemote)||
                       (obj.state == MSFSObject::ObjectState::ChangedLocal)||
                       (obj.state == MSFSObject::ObjectState::ChangedRemote) ){

                        if(obj.remote.exist){

                            if(obj.local.objectType == MSLocalFSObject::Type::file){

                                qStdOut()<< obj.path<<obj.fileName <<" Forced uploading." <<endl;

                                this->remote_file_update(&obj);
                            }
                        }
                        else{

                            if(obj.local.objectType == MSLocalFSObject::Type::file){

                                qStdOut()<< obj.path<<obj.fileName <<" Forced uploading." <<endl;

                                this->remote_file_insert(&obj);
                            }
                        }


                    }

                }
            }
            else{
                // error
            }
        }


        if(this->getFlag("dryRun")){
            return;
        }

        // save state file

        this->saveStateFile();



            qStdOut()<<"Syncronization end" <<endl;

            return;
    }



    // SYNC FILES AND FOLDERS

    qStdOut()<<"Start syncronization" <<endl;

    lf=this->syncFileList.begin();

    for(;lf != this->syncFileList.end();lf++){

        MSFSObject obj=lf.value();

        if((obj.state == MSFSObject::ObjectState::Sync)){

            continue;
        }

        switch((int)(obj.state)){

            case MSFSObject::ObjectState::ChangedLocal:

                qStdOut()<< obj.path<<obj.fileName <<" Changed local. Uploading." <<endl;

                this->remote_file_update(&obj);

                break;

            case MSFSObject::ObjectState::NewLocal:

                if((obj.local.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                    this->remote_file_insert(&obj);

                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);

                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Delete local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }


                    }
                }


                break;

            case MSFSObject::ObjectState::ChangedRemote:

                qStdOut()<< obj.path<<obj.fileName <<" Changed remote. Downloading." <<endl;

                this->remote_file_get(&obj);

                break;


            case MSFSObject::ObjectState::NewRemote:

                if((obj.remote.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" Delete local. Deleting remote." <<endl;

                        this->remote_file_trash(&obj);

                    }
                    else{
                        qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                        this->remote_file_get(&obj);
                    }


                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" Delete local. Deleting remote." <<endl;

                        this->remote_file_trash(&obj);
                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                        this->remote_file_get(&obj);
                    }
                }

                break;


            case MSFSObject::ObjectState::DeleteLocal:

                if((obj.remote.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                    this->remote_file_get(&obj);

                    break;
                }

                qStdOut()<< obj.fileName <<" Delete local. Deleting remote." <<endl;

                this->remote_file_trash(&obj);

                break;

            case MSFSObject::ObjectState::DeleteRemote:

                if((obj.local.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);
                    }
                    else{
                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Deleting local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }

                    }
                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);

                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Deleting local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }


                    }
                }


                break;


        }


    }

    if(this->getFlag("dryRun")){
        return;
    }

    // save state file

    this->saveStateFile();




        qStdOut()<<"Syncronization end" <<endl;

}


bool MSYandexDisk::remote_file_generateIDs(int count) {
// absolete
    // fix warning message
    count++;
    return false;
}

QHash<QString,MSFSObject>   MSYandexDisk::filelist_getFSObjectsByState(MSFSObject::ObjectState state) {

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

    while(i != this->syncFileList.end()){

        if(i.value().state == state){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;

}


QHash<QString,MSFSObject>   MSYandexDisk::filelist_getFSObjectsByState(QHash<QString,MSFSObject> fsObjectList,MSFSObject::ObjectState state) {

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=fsObjectList.begin();

    while(i != fsObjectList.end()){

        if(i.value().state == state){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;
}


QHash<QString,MSFSObject>   MSYandexDisk::filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type type){


    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

    while(i != this->syncFileList.end()){

        if(i.value().local.objectType == type){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;
}


QHash<QString,MSFSObject>   MSYandexDisk::filelist_getFSObjectsByTypeRemote(MSRemoteFSObject::Type type){

        QHash<QString,MSFSObject> out;

        QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

        while(i != this->syncFileList.end()){

            if(i.value().remote.objectType == type){
                out.insert(i.key(),i.value());
            }

            i++;
        }

        return out;

}

//=======================================================================================

bool MSYandexDisk::filelist_FSObjectHasParent(MSFSObject fsObject){
    if(fsObject.path=="/"){
        return false;
    }
    else{
        return true;
    }

    if(fsObject.path.count("/")>=1){
        return true;
    }
    else{
        return false;
    }

}

//=======================================================================================



MSFSObject MSYandexDisk::filelist_getParentFSObject(MSFSObject fsObject){

    QString parentPath;

    if((fsObject.local.objectType==MSLocalFSObject::Type::file) || (fsObject.remote.objectType==MSRemoteFSObject::Type::file)){
        parentPath=fsObject.path.left(fsObject.path.lastIndexOf("/"));
    }
    else{
        parentPath=fsObject.path.left(fsObject.path.lastIndexOf("/"));
    }

    if(parentPath==""){
        parentPath="/";
    }

    QHash<QString,MSFSObject>::iterator parent=this->syncFileList.find(parentPath);

    if(parent != this->syncFileList.end()){
        return parent.value();
    }
    else{
        return MSFSObject();
    }
}


void MSYandexDisk::filelist_populateChanges(MSFSObject changedFSObject){

    QHash<QString,MSFSObject>::iterator object=this->syncFileList.find(changedFSObject.path+changedFSObject.fileName);

    if(object != this->syncFileList.end()){
        object.value().local=changedFSObject.local;
        object.value().remote.data=changedFSObject.remote.data;
    }
}



bool MSYandexDisk::testReplyBodyForError(QString body) {

        QJsonDocument json = QJsonDocument::fromJson(body.toUtf8());
        QJsonObject job = json.object();

        QJsonValue e=(job["error"]);
        if(e.isNull()){
            return true;
        }

        return false;


}


QString MSYandexDisk::getReplyErrorString(QString body) {

    QJsonDocument json = QJsonDocument::fromJson(body.toUtf8());
    QJsonObject job = json.object();

    QJsonValue e=(job["description"]);

    return e.toString();

}



//=======================================================================================

// download file from cloud
bool MSYandexDisk::remote_file_get(MSFSObject* object){

    if(this->getFlag("dryRun")){
        return true;
    }

    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources/download");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));
    req->addQueryItem("path",                           object->path+object->fileName);

    req->exec();

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }

    QString filePath=this->workPath+object->path+object->fileName;

    QString content=req->readReplyText();


    if(this->testReplyBodyForError(req->readReplyText())){

        if(object->remote.objectType==MSRemoteFSObject::Type::file){

            QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
            QJsonObject job = json.object();
            QString href=job["href"].toString();

            this->local_writeFileContent(filePath,href);// call overloaded from this

            // set remote "change time" for local file

//            utimbuf tb;
//            tb.actime=(this->toMilliseconds(object->remote.data["client_modified"].toString(),true))/1000;;
//            tb.modtime=(this->toMilliseconds(object->remote.data["client_modified"].toString(),true))/1000;;

//            utime(filePath.toStdString().c_str(),&tb);
        }
    }
    else{

        if(! this->getReplyErrorString(req->readReplyText()).contains( "path/not_file/")){
            qStdOut() << "Service error. "<< this->getReplyErrorString(req->readReplyText());
            delete(req);
            return false;
        }
    }


    delete(req);
    return true;

}

//=======================================================================================

bool MSYandexDisk::remote_file_insert(MSFSObject *object){

    if(object->local.objectType==MSLocalFSObject::Type::folder){

        qStdOut()<< object->fileName << " is a folder. Skipped." <<endl;
        return true;
    }

    if(this->getFlag("dryRun")){
        return true;
    }


    // get url for uploading ===========

    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources/upload");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));
    req->addQueryItem("path",                           object->path+object->fileName);
    req->addQueryItem("overwrite",                           "true");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }



    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString href=job["href"].toString();



    delete(req);

    // upload file content

    QString filePath=  this->workPath+object->path+object->fileName;

    QFile file(filePath);
    qint64 fSize=file.size();

    req = new MSRequest(this->proxyServer);

    req->setRequestUrl(href);


    if (!file.open(QIODevice::ReadOnly)){

        //error file not found
        qStdOut()<<"Unable to open of "+filePath <<endl;
        delete(req);
        return false;
    }


    req->addHeader("Content-Length",QString::number(fSize).toLocal8Bit());
    req->put(&file);

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }




    delete(req);
    return true;
}


//=======================================================================================

bool MSYandexDisk::remote_file_update(MSFSObject *object){

    if(object->local.objectType==MSLocalFSObject::Type::folder){

        qStdOut()<< object->fileName << " is a folder. Skipped." <<endl;
        return true;
    }

    if(this->getFlag("dryRun")){
        return true;
    }


    // get url for uploading ===========

    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources/upload");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));
    req->addQueryItem("path",                           object->path+object->fileName);
    req->addQueryItem("overwrite",                           "true");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }



    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString href=job["href"].toString();



    delete(req);

    // upload file content

    QString filePath=  this->workPath+object->path+object->fileName;

    QFile file(filePath);
    qint64 fSize=file.size();

    req = new MSRequest(this->proxyServer);

    req->setRequestUrl(href);


    if (!file.open(QIODevice::ReadOnly)){

        //error file not found
        qStdOut()<<"Unable to open of "+filePath <<endl;
        delete(req);
        return false;
    }


    req->addHeader("Content-Length",QString::number(fSize).toLocal8Bit());
    req->put(&file);

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        return false;
    }




    delete(req);
    return true;
}

//=======================================================================================

bool MSYandexDisk::remote_file_makeFolder(MSFSObject *object){

    if(this->getFlag("dryRun")){
        return true;
    }

    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addQueryItem("path",                           object->path+object->fileName);

    QByteArray metaData;


    req->put(metaData);


    if(!req->replyOK()){

        if(!req->replyErrorText.contains("CONFLICT")){ // skip error on re-create existing folder

            req->printReplyError();
            delete(req);
            //exit(1);
            return false;
        }
        else{
            delete(req);
            return true;
        }

    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        delete(req);
        return false;
    }



    // getting meta info of created folder


    QString filePath=this->workPath+object->path+object->fileName;

    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();


    QString href=job["href"].toString();

    delete(req);

    req = new MSRequest(this->proxyServer);

    req->setRequestUrl(href);
    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addQueryItem("path",                           object->path+object->fileName);
    req->setMethod("get");

    req->exec();

    content=req->readReplyText();

    json = QJsonDocument::fromJson(content.toUtf8());
    job = json.object();

   // object->local.newRemoteID=job["id"].toString();
    object->remote.data=job;
    object->remote.exist=true;

    if(job["path"].toString()==""){
        qStdOut()<< "Error when folder create "+filePath+" on remote" <<endl;
    }

    delete(req);

    this->filelist_populateChanges(*object);

    return true;

}

//=======================================================================================

void MSYandexDisk::remote_file_makeFolder(MSFSObject *object, QString parentID){
// obsolete
    //fix warning message
    object=object;
    parentID+="";

}

//=======================================================================================

bool MSYandexDisk::remote_file_trash(MSFSObject *object){

    if(this->getFlag("dryRun")){
        return true;
    }


    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources");


    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    //req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));
    req->addQueryItem("path",                           object->path+object->fileName);

    req->deleteResource();

    if(!this->testReplyBodyForError(req->readReplyText())){
        QString errt=this->getReplyErrorString(req->readReplyText());

        if(! errt.contains("Resource not found")){// ignore previous deleted files

            qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
            delete(req);
            return false;
        }
        else{
            delete(req);
            return true;
        }


    }




    delete(req);
    return true;
}

//=======================================================================================

bool MSYandexDisk::remote_createDirectory(QString path){

    if(this->getFlag("dryRun")){
        return true;
    }


    QList<QString> dirs=path.split("/");
    QString currPath="";

    for(int i=1;i<dirs.size();i++){

        QHash<QString,MSFSObject>::iterator f=this->syncFileList.find(currPath+"/"+dirs[i]);

        if(f != this->syncFileList.end()){

            currPath=f.key();

            if(f.value().remote.exist){
                continue;
            }

            if(this->filelist_FSObjectHasParent(f.value())){

//                MSFSObject parObj=this->filelist_getParentFSObject(f.value());
//                this->remote_file_makeFolder(&f.value(),parObj.remote.data["id"].toString());
                this->remote_file_makeFolder(&f.value());

            }
            else{

                this->remote_file_makeFolder(&f.value());
            }
        }

    }
    return true;
}






// ============= LOCAL FUNCTIONS BLOCK =============
//=======================================================================================

void MSYandexDisk::local_createDirectory(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir d;
    d.mkpath(path);

}

//=======================================================================================

void MSYandexDisk::local_removeFile(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir trash(this->workPath+"/"+this->trashFileName);

    if(!trash.exists()){
        trash.mkdir(this->workPath+"/"+this->trashFileName);
    }

    QString origPath=this->workPath+path;
    QString trashedPath=this->workPath+"/"+this->trashFileName+path;

    QFile f;
    f.setFileName(origPath);
    bool res=f.rename(trashedPath);

    if((!res)&&(f.exists())){// everwrite trashed file
        QFile ef(trashedPath);
        ef.remove();
        f.rename(trashedPath);
    }

}

//=======================================================================================

void MSYandexDisk::local_removeFolder(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir trash(this->workPath+"/"+this->trashFileName);

    if(!trash.exists()){
        trash.mkdir(this->workPath+"/"+this->trashFileName);
    }

    QString origPath=this->workPath+path;
    QString trashedPath=this->workPath+"/"+this->trashFileName+path;

    QDir f;
    f.setPath(origPath);
    bool res=f.rename(origPath,trashedPath);

    if((!res)&&(f.exists())){// everwrite trashed folder
        QDir ef(trashedPath);
        ef.removeRecursively();
        f.rename(origPath,trashedPath);
    }

}

//=======================================================================================

bool MSYandexDisk::local_writeFileContent(QString filePath, QString  hrefToDownload){


    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl(hrefToDownload);
    req->setMethod("get");

    req->download(hrefToDownload);

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        exit(1);
    }


    QFile file(filePath);
    file.open(QIODevice::WriteOnly );
    QDataStream outk(&file);

    QByteArray ba;
    ba.append(req->readReplyText());

    int sz=ba.size();


    outk.writeRawData(ba.data(),sz) ;

    file.close();
    return true;
}



bool MSYandexDisk::directUpload(QString url, QString remotePath){

    // download file into temp file ---------------------------------------------------------------

    MSRequest *req = new MSRequest(this->proxyServer);

    QString filePath=this->workPath+"/"+this->generateRandom(10);

    req->setMethod("get");
    req->download(url,filePath);


    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        exit(1);
    }

    QFileInfo fileRemote(remotePath);
    QString path=fileRemote.absoluteFilePath();

    path=QString(path).left(path.lastIndexOf("/"));

    MSFSObject object;
    object.path=path+"/";
    object.fileName=fileRemote.fileName();

    this->syncFileList.insert(object.path+object.fileName,object);

    if(path!="/"){

        QStringList dirs=path.split("/");

        MSFSObject* obj=0;
        QString cPath="/";

        for(int i=1;i<dirs.size();i++){

            obj=new MSFSObject();
            obj->path=cPath+dirs[i];
            obj->fileName="";
            obj->remote.exist=false;
            this->remote_file_makeFolder(obj);
            cPath+=dirs[i]+"/";
            delete(obj);

        }


    }



    delete(req);

    // upload file to remote ------------------------------------------------------------------------


    // get url for uploading ===========

    req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/resources/upload");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));
    req->addQueryItem("path",                           object.path+object.fileName);
    req->addQueryItem("overwrite",                           "true");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        exit(1);
    }



    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString href=job["href"].toString();



    delete(req);

    // upload file content

   // filePath=  this->workPath+object.path+object.fileName;

    QFile file(filePath);
    qint64 fSize=file.size();

    req = new MSRequest(this->proxyServer);

    req->setRequestUrl(href);


    if (!file.open(QIODevice::ReadOnly)){

        //error file not found
        qStdOut()<<"Unable to open of "+filePath <<endl;
        delete(req);
        exit(1);
    }


    req->addHeader("Content-Length",QString::number(fSize).toLocal8Bit());
    req->put(&file);

    if(!req->replyOK()){
        req->printReplyError();
        delete(req);
        exit(1);
    }


    file.remove();

    delete(req);

    return true;
}




QString MSYandexDisk::getInfo(){

    MSRequest *req0 = new MSRequest(this->proxyServer);

    req0->setRequestUrl("https://login.yandex.ru/info");
    req0->setMethod("get");

   // req0->addHeader("Authorization",                     "OAuth "+this->access_token);
   // req0->addHeader("Accept:",                      QString("*/*"));

    req0->addQueryItem("oauth_token",QString(this->access_token));


    req0->exec();


    if(!req0->replyOK()){
        req0->printReplyError();

        qStdOut()<< req0->replyText;
        delete(req0);
        return "false";
    }

    if(!this->testReplyBodyForError(req0->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req0->readReplyText()) << endl;
        delete(req0);
        return "false";
    }


    QString content0=req0->readReplyText();

    QJsonDocument json0 = QJsonDocument::fromJson(content0.toUtf8());
    QJsonObject job0 = json0.object();





    MSRequest *req = new MSRequest(this->proxyServer);

    req->setRequestUrl("https://cloud-api.yandex.net/v1/disk/");
    req->setMethod("get");

    req->addHeader("Authorization",                     "OAuth "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));



    req->exec();


    if(!req->replyOK()){
        req->printReplyError();

        qStdOut()<< req->replyText;
        delete(req);
        return "false";
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        delete(req);
        return "false";
    }


    QString content=req->readReplyText();
    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();

    if(((uint64_t)job["total_space"].toDouble()) == 0){
        //qStdOut()<< "Error getting cloud information " <<endl;
        return "false";
    }


    QJsonObject out;
    out["account"]=job0["login"].toString()+"@yandex.ru";
    out["total"]= QString::number( (uint64_t)job["total_space"].toDouble());
    out["usage"]= QString::number( job["used_space"].toInt());

    return QString( QJsonDocument(out).toJson());

}





