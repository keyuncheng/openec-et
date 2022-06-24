#include "FSObjInputStream.hh"

FSObjInputStream::FSObjInputStream(Config* conf, string objname, UnderFS* fs) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  _conf = conf;
  _objname = objname;
  _queue = new BlockingQueue<OECDataPacket*>();
  _dataPktNum = 0;

  _underfs = fs;
  _underfile = _underfs->openFile(objname, "read");
  if (!_underfile) {
    _exist = false;
  } else {
//    _exist = true;
    _objbytes = _underfs->getFileSize(_underfile);
    cout << "FSObjInputStream::constructor.objsize = " << _objbytes << endl;
    _offset = 0;
    if (_objbytes == 0) _exist = false;
    else _exist = true;
  }
  gettimeofday(&time2, NULL);
  cout << "FSObjInputStream::constructor.time = " << RedisUtil::duration(time1, time2) << endl;
}

FSObjInputStream::~FSObjInputStream() {
  if (_queue) delete _queue;
  if (_underfile) _underfs->closeFile(_underfile);
}

void FSObjInputStream::readObj(int slicesize) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  while(true) {
    int hasread = 0;
    char* buf = (char*)calloc(slicesize+4, sizeof(char));
    if (!buf) {
      cout << "FSObjInputStream::readObj.malloc buffer fail" << endl;
      return;
    }
    while(hasread < slicesize) {
      int len = _underfs->readFile(_underfile, buf+4+hasread, slicesize-hasread);
      if (len == 0) break;
      hasread += len;
    }

    // set hasread in the first 4 bytes of buf
    int tmplen = htonl(hasread);
    memcpy(buf, (char*)&tmplen, 4);

    if (hasread) {
      OECDataPacket* curPkt = new OECDataPacket();
      curPkt->setRaw(buf);
      _queue->push(curPkt); _dataPktNum++;
    }
    if (hasread <= 0) break;
  }
  gettimeofday(&time2, NULL);

  // cout << fixed << setprecision(0) << "FSObjInputStream.readObj: " <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2)
  //  << " for " << _objname << " of " << _dataPktNum << " slices"<< endl;

  cout << "FSObjInputStream.readObj.duration = " << RedisUtil::duration(time1, time2) << " for " << _objname << " of " << _dataPktNum << " slices"<< endl;
}

void FSObjInputStream::readObj() {

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  while(true) {
    int hasread = 0;
    char* buf = (char*)calloc(_conf->_pktSize+4, sizeof(char));
    if (!buf) {
      cout << "FSObjInputStream::readObj.malloc buffer fail" << endl;
      return;
    }
    while(hasread < _conf->_pktSize) {
      int len = _underfs->readFile(_underfile, buf+4+hasread, _conf->_pktSize-hasread);
      if (len == 0) break;
      hasread += len;
    }

    // set hasread in the first 4 bytes of buf
    int tmplen = htonl(hasread);
    memcpy(buf, (char*)&tmplen, 4);

    if (hasread) {
      OECDataPacket* curPkt = new OECDataPacket();
      curPkt->setRaw(buf);
      _queue->push(curPkt); _dataPktNum++;
    }
    if (hasread <= 0) break;
  }
  gettimeofday(&time2, NULL);

  //   cout << fixed << setprecision(0) << "FSObjInputStream.readObj: " <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2)
  //  << " for " << _objname << ", pktnum: " << _dataPktNum << endl;

  cout << "FSObjInputStream.readObj.duration = " << RedisUtil::duration(time1, time2) << " for " << _objname << ", pktnum: " << _dataPktNum << endl;
}

void FSObjInputStream::readObj(int w, vector<int> list, int slicesize) {
  if (w == 1) {
    readObj();
    return;
  }

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  // we first transfer items in list %w
  vector<int> offsetlist;
  for (int i=0; i<list.size(); i++) offsetlist.push_back(list[i]%w);
  sort(offsetlist.begin(), offsetlist.end());

  // for each w slices, we put those slice whose index is in offsetlist
  int stripeid=0;
  int pktsize = _conf->_pktSize;
  int stripenum = _objbytes / pktsize;
  cout << "FSObjInputStream::readObj.stripenum:  " << stripenum << endl;
  int slicenum = 0;
  while (stripeid < stripenum) {
    int start = stripeid * pktsize;
    for (int i=0; i<offsetlist.size(); i++) {
      int offidx = offsetlist[i];
      int slicestart = start + offidx * slicesize;
      char* buf = (char*)calloc(slicesize+4, sizeof(char));
  
      int hasread = 0;
      while(hasread < slicesize) {
        int len = _underfs->pReadFile(_underfile, slicestart + hasread, buf+4 + hasread, slicesize - hasread);
        if (len == 0)break;
        hasread += len;
      }

      // set hasread in the first 4 bytes of buf
      int tmplen = htonl(hasread);
      memcpy(buf, (char*)&tmplen, 4);

      if (hasread) {
        OECDataPacket* curPkt = new OECDataPacket();
        curPkt->setRaw(buf);
        _queue->push(curPkt); slicenum++;
      }
    }
    stripeid++;
  }
  gettimeofday(&time2, NULL);

  // cout << fixed << setprecision(0) << "FSObjInputStream.readObj: " <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2)
  //  << " for " << _objname << ", totally " << slicenum << "slices" << endl;

  cout << "FSObjInputStream.readObj.duration = " << RedisUtil::duration(time1, time2) << " for " << _objname << ", totally " << slicenum << "slices" << endl;
}

void FSObjInputStream::readObjOptimized(int w, vector<int> list, int slicesize) {
  if (w == 1) {
    readObj();
    return;
  }

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);

  // we first transfer items in list %w
  vector<int> offset_list;
  for (int i = 0; i < list.size(); i++) {
    offset_list.push_back(list[i]%w); 
  }
  sort(offset_list.begin(), offset_list.end()); // sort in ascending order
  reverse(offset_list.begin(), offset_list.end());
  
  // create consecutive read list
  int num_of_cons_reads = 0;
  vector<int> cons_list;
  vector<vector<int>> cons_read_list; // consecutive read list
  while (offset_list.empty() == false) { 
    int offset = offset_list.back();
    offset_list.pop_back();

    if (cons_list.empty() == true) {
      cons_list.push_back(offset);
    } else {
      // it's consecutive
      if (cons_list.back() + 1 == offset) {
        cons_list.push_back(offset); // at to the back of prev cons_list
      } else {
        cons_read_list.push_back(cons_list); // commits prev cons_list
        cons_list.clear();
        cons_list.push_back(offset); // at to the back of new cons_list
      }
    }
  }
  cons_read_list.push_back(cons_list);

  printf("cons_read_list:\n");
  for (auto cons_list : cons_read_list) {
    for (auto offset : cons_list) {
      printf("%d ", offset);
    }
    printf("\n");
  }

  // for each w slices, we put those slice whose index is in offsetlist
  int stripeid=0;
  int pktsize = _conf->_pktSize;
  int stripenum = _objbytes / pktsize;
  cout << "FSObjInputStream::readObj.stripenum:  " << stripenum << endl;
  int slicenum = 0;
  // for each stripe, read packets consecutively for each cons_list
  while (stripeid < stripenum) {
    int stripe_start = stripeid * pktsize;

    for (auto cons_list : cons_read_list) {
      int offset_start = cons_list[0];
      int slice_start = stripe_start + offset_start * slicesize;
      int num_cons_read_packets = cons_list.size();
      int read_size = num_cons_read_packets * slicesize;
      char *read_cons_buf = (char *)calloc(read_size, sizeof(char));

      int bytes_read = 0;
      while (bytes_read < read_size) {
        int len = 0;
        if (num_cons_read_packets == w) {
          // if read the whole packet, resort to sequential instead
          len = _underfs->readFile(_underfile, read_cons_buf + bytes_read, read_size - bytes_read);
        } else {
          // partial read
          len = _underfs->pReadFile(_underfile, slice_start + bytes_read, read_cons_buf + bytes_read, read_size - bytes_read);
        }
        
        if (len == 0)break;
        bytes_read += len;
      }

      if (bytes_read % num_cons_read_packets != 0) {
        printf("error: undevisible sub-packets read, bytes_read: %d, num_cons_read_packets: %d\n", bytes_read, num_cons_read_packets);
      }

      int read_pkt_size = bytes_read / num_cons_read_packets;
      if (read_pkt_size <= 0) {
        continue;
      }

      for (int i = 0; i < num_cons_read_packets; i++) {
        char *pkt_buf = (char *)calloc(4 + slicesize, sizeof(char));
        // set read_pkt_size in the first 4 bytes of pkt_buf
        int tmplen = htonl(read_pkt_size);
        memcpy(pkt_buf, (char*)&tmplen, 4);

        // copy data to pkt_buf from read_cons_buf
        memcpy(pkt_buf + 4, read_cons_buf + i * read_pkt_size, read_pkt_size * sizeof(char));
        
        OECDataPacket* curPkt = new OECDataPacket();
        curPkt->setRaw(pkt_buf);
        _queue->push(curPkt);
        slicenum++;
      }
    }
    stripeid++;
  }
  gettimeofday(&time2, NULL);

  // cout << fixed << setprecision(0) << "FSObjInputStream.readObj: " <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2)
  //  << " for " << _objname << ", totally " << slicenum << "slices" << endl;

  cout << "FSObjInputStream.readObjOptimized.duration = " << RedisUtil::duration(time1, time2) << " for " << _objname << ", totally " << slicenum << "slices" << endl;
}

void FSObjInputStream::readObj(int slicesize, int unitIdx) {

  if (slicesize == _conf->_pktSize) {
    readObj();
    return;
  }

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  int pktnum = 0;
  while(true) {
    int hasread = 0;
    long objoffset = pktnum * _conf->_pktSize + unitIdx * slicesize;

    char* buf = (char*)calloc(slicesize+4, sizeof(char));
    if (!buf) {
      cout << "FSObjInputStream::readObj.malloc buffer fail" << endl;
      return;
    }

    while(hasread < slicesize) {
      int len = _underfs->pReadFile(_underfile, objoffset + hasread, buf+4 + hasread, slicesize - hasread);
      if (len == 0) break;
      hasread += len;
    }

    // set hasread in the first 4 bytes of buf
    int tmplen = htonl(hasread);
    memcpy(buf, (char*)&tmplen, 4);

    if (hasread) {
      OECDataPacket* curPkt = new OECDataPacket();
      curPkt->setRaw(buf);
      _queue->push(curPkt); pktnum++;
    }

    if (hasread <= 0) break;
  }

  gettimeofday(&time2, NULL);

  // cout << fixed << setprecision(0) << "FSObjInputStream.readObj: " <<
  // ", start: " << time1.tv_sec * 1000.0 + time1.tv_usec / 1000.0 <<
  // ", end: " << time2.tv_sec * 1000.0 + time2.tv_usec / 1000.0 <<
  // ", duration: " << RedisUtil::duration(time1, time2)
  //  << " for " << _objname << ", pktnum = " << pktnum << endl;


  cout << "FSObjInputStream.readObj.duration = " << RedisUtil::duration(time1, time2) << " for " << _objname << ", pktnum = " << pktnum << endl;
}

OECDataPacket* FSObjInputStream::dequeue() {
  OECDataPacket* toret = _queue->pop();
  _offset += toret->getDatalen();
  return toret;
}

bool FSObjInputStream::exist() {
  return _exist;
}

bool FSObjInputStream::hasNext() {
  return (_offset < _objbytes) ? true:false;
}

int FSObjInputStream::pread(long objoffset, char* buffer, int buflen) {
  int hasread = 0;
  while (hasread < buflen) {
    int len = _underfs->pReadFile(_underfile, objoffset + hasread, buffer+hasread, buflen - hasread);
    hasread += len;
  }
  return hasread;
}

BlockingQueue<OECDataPacket*>* FSObjInputStream::getQueue() {
  return _queue;
}
