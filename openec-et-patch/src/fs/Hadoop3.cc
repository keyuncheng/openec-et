#include "Hadoop3.hh"

#include <iomanip>

Hadoop3::Hadoop3(vector<string> params, Config* conf) {
  cout << "hadoop3 constructor!" << endl;
  _ip = params[0];
  _port = atoi(params[1].c_str());
  _fs = hdfsConnect(_ip.c_str(), _port);

  _conf = conf;
}

Hadoop3::~Hadoop3() {
//  cout << "Hadoop3::~Hadoop3" << endl;
  hdfsDisconnect(_fs);
} 

Hadoop3File* Hadoop3::openFile(string filename, string mode) {
  Hadoop3File* toret = NULL;
  hdfsFile underfile;
  if (mode == "read") {
    cout << "Hadoop3.openFile for read" << endl;
    underfile = hdfsOpenFile(_fs, filename.c_str(), O_RDONLY, _conf->_pktSize, 0, 0);
  } else {
    cout << "Hadoop3.openFile "<<filename<<" for write" << endl;
    underfile = hdfsOpenFile(_fs, filename.c_str(), O_WRONLY | O_CREAT, 0, 0, 0);
    hdfsCloseFile(_fs, underfile);
    underfile = hdfsOpenFile(_fs, filename.c_str(), O_WRONLY | O_APPEND, 0, 0, 0);
  }
  if (!underfile) {
    cerr << "Failed to open " << filename << " in Hadoop3" << endl;
    toret = NULL;
  } else {
    toret = new Hadoop3File(filename, underfile);
  }
  return toret;
}

void Hadoop3::writeFile(UnderFile* file, char* buffer, int len) {
//  cout << "Hadoop3.writeFile , len = " << len << endl;
  hdfsWrite(_fs, ((Hadoop3File*)file)->_objfile, (void*)buffer, len);
}

void Hadoop3::flushFile(UnderFile* file) {
//  cout << "Hadoop3::flushFile" << endl;
  if (hdfsFlush(_fs, ((Hadoop3File*)file)->_objfile)) {
    cerr << "Failed to 'flush' " << ((Hadoop3File*)file)->_objname << endl;
    exit(-1);
  }
}

void Hadoop3::closeFile(UnderFile* file) {
//  cout << "Hadoop3::closeFile" << endl;
  hdfsFile objfile = ((Hadoop3File*)file)->_objfile;
  if (objfile) {
    hdfsCloseFile(_fs, objfile);
    ((Hadoop3File*)file)->_objfile = NULL;
  }
  delete file;
}

int Hadoop3::readFile(UnderFile* file, char* buffer, int len) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  int retval = hdfsRead(_fs, ((Hadoop3File*)file)->_objfile, buffer, len);

  gettimeofday(&time2, NULL);
  // cout << fixed << setprecision(0) << "Hadoop3::readFile, name: " << ((Hadoop3File*)file)->_objname << ", len:" << len <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2) << endl;

  return retval;
}

int Hadoop3::pReadFile(UnderFile* file, int offset, char* buffer, int len) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  int retval = hdfsPread(_fs, ((Hadoop3File*)file)->_objfile, offset, buffer, len);

  gettimeofday(&time2, NULL);

  // cout << fixed << setprecision(0) << "Hadoop3::pReadFile, name: " << ((Hadoop3File*)file)->_objname << ", offset: " << offset <<  ", len:" << len <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2) << endl;

  return retval;
}

int Hadoop3::getFileSize(UnderFile* file) {
//  cout << "Hadoop3::getFileSize" << endl;
  hdfsFileInfo* fileinfo = hdfsGetPathInfo(_fs, ((Hadoop3File*)file)->_objname.c_str());
  return fileinfo->mSize;
}
