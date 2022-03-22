//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id$
//
// Created 2006/06/06
// Author: Sriram Rao
//         Mike Ovsiannikov
//
// Copyright 2008-2012,2016 Quantcast Corporation. All rights reserved.
// Copyright 2006-2008 Kosmix Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// \file LayoutManager.h
// \brief Layout manager is responsible for laying out chunks on chunk
// servers.  Model is that, when a chunkserver connects to the meta
// server, the layout manager gets notified; the layout manager then
// uses the chunk server for data placement.
//
//----------------------------------------------------------------------------

#ifndef META_LAYOUTMANAGER_H
#define META_LAYOUTMANAGER_H

#include "kfstypes.h"
#include "meta.h"
#include "ChunkServer.h"
#include "UserAndGroup.h"
#include "MetaRequest.h"
#include "CSMap.h"
#include "ChunkPlacement.h"
#include "AuthContext.h"
#include "IdempotentRequestTracker.h"

#include "common/Properties.h"
#include "common/StdAllocator.h"
#include "common/kfsatomic.h"
#include "common/StTmp.h"
#include "common/HostPrefix.h"
#include "common/LinearHash.h"
#include "common/StBuffer.h"
#include "common/TimerWheel.h"
#include "common/BufferInputStream.h"
#include "common/PoolAllocator.h"
#include "common/SingleLinkedQueue.h"

#include "common/CoorCommand.h"

#include "qcdio/QCDLList.h"

#include "kfsio/Counter.h"
#include "kfsio/KfsCallbackObj.h"
#include "kfsio/ITimeout.h"
#include "kfsio/event.h"
#include "kfsio/Globals.h"
#include "kfsio/PrngIsaac64.h"
#include "kfsio/DelegationToken.h"

#include <map>
#include <vector>
#include <set>
#include <deque>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <string.h>

#include <boost/bind.hpp>

class QCIoBufferPool;
namespace KFS
{
using std::string;
using std::map;
using std::vector;
using std::pair;
using std::list;
using std::set;
using std::make_pair;
using std::less;
using std::equal_to;
using std::deque;
using std::ostream;
using std::find;
using std::ifstream;
using std::lower_bound;
using std::ostringstream;
using std::min;
using boost::bind;
using libkfsio::globalNetManager;

/// Model for leases: metaserver assigns write leases to chunkservers;
/// clients/chunkservers can grab read lease on a chunk at any time.
/// The server will typically renew unexpired leases whenever asked.
/// As long as the lease is valid, server promises not to chnage
/// the lease's version # (also, chunk won't disappear as long as
/// lease is valid).
class ARAChunkCache;
class ChunkLeases
{
public:
    class EntryKey : public pair<chunkId_t, chunkOff_t>
    {
    public:
        EntryKey()
            : pair<chunkId_t, chunkOff_t>()
            {}
        EntryKey(fid_t fid, chunkOff_t pos)
            : pair<chunkId_t, chunkOff_t>(fid, pos < 0 ? chunkOff_t(-1) :
                chunkStartOffset(pos))
            {}
        EntryKey(chunkId_t fid)
            : pair<chunkId_t, chunkOff_t>(fid, -1)
            {}
        bool IsChunkEntry() const
            { return (second < 0); }
    };
    enum { kLeaseTimerResolutionSec = 4 }; // Power of two to optimize division.
    typedef int64_t LeaseId;
    typedef DelegationToken::TokenSeq TokenSeq;
    struct ReadLease
    {
        ReadLease(LeaseId id, time_t exp)
            : leaseId(id),
              expires(exp)
            {}
        ReadLease(const ReadLease& lease)
            : leaseId(lease.leaseId),
              expires(lease.expires)
            {}
        ReadLease()
            : leaseId(-1),
              expires(0)
            {}
        const LeaseId leaseId;
        time_t        expires;
        time_t GetExpiration() const
            { return expires; }
    };
    struct WriteLease : public ReadLease
    {
        WriteLease(
            LeaseId             i,
            time_t              exp,
            const MetaAllocate& alloc)
            : ReadLease(i, exp),
              chunkVersion(alloc.chunkVersion),
              chunkServer(alloc.servers.front()),
              pathname(alloc.pathname.GetStr()),
              appendFlag(alloc.appendChunk),
              stripedFileFlag(alloc.stripedFileFlag),
              relinquishedFlag(false),
              ownerWasDownFlag(false),
              allocInFlight(&alloc),
              euser(alloc.euser),
              egroup(alloc.egroup),
              endTime(alloc.sessionEndTime),
              delegationSeq(alloc.delegationSeq),
              delegationValidForTime(alloc.validDelegationFlag ?
                alloc.delegationValidForTime : uint32_t(0)),
              delegationFlags(alloc.delegationFlags),
              delegationIssuedTime(alloc.delegationIssuedTime),
              delegationUser(alloc.authUid)
            {}
        WriteLease(const WriteLease& lease)
            : ReadLease(lease),
              chunkVersion(lease.chunkVersion),
              chunkServer(lease.chunkServer),
              pathname(lease.pathname),
              appendFlag(lease.appendFlag),
              stripedFileFlag(lease.stripedFileFlag),
              relinquishedFlag(lease.relinquishedFlag),
              ownerWasDownFlag(lease.ownerWasDownFlag),
              allocInFlight(lease.allocInFlight),
              euser(lease.euser),
              egroup(lease.egroup),
              endTime(lease.endTime),
              delegationSeq(lease.delegationSeq),
              delegationValidForTime(lease.delegationValidForTime),
              delegationFlags(lease.delegationFlags),
              delegationIssuedTime(lease.delegationIssuedTime),
              delegationUser(lease.delegationUser)
            {}
        WriteLease()
            : ReadLease(),
              chunkVersion(-1),
              chunkServer(),
              pathname(),
              appendFlag(false),
              stripedFileFlag(false),
              relinquishedFlag(false),
              ownerWasDownFlag(false),
              allocInFlight(0),
              euser(kKfsUserNone),
              egroup(kKfsGroupNone),
              endTime(0),
              delegationSeq(-1),
              delegationValidForTime(0),
              delegationFlags(0),
              delegationIssuedTime(0),
              delegationUser(kKfsUserNone)
            {}
        void ResetServer()
            { const_cast<ChunkServerPtr&>(chunkServer).reset(); }
        const seq_t          chunkVersion;
        const ChunkServerPtr chunkServer;
        // record the pathname; we can use the path to traverse
        // the dir. tree and update space used at each level of
        // the tree
        const string         pathname;
        const bool           appendFlag:1;
        const bool           stripedFileFlag:1;
        bool                 relinquishedFlag:1;
        bool                 ownerWasDownFlag:1;
        const MetaAllocate*  allocInFlight;
        kfsUid_t             euser;
        kfsGid_t             egroup;
        int64_t              endTime;
        TokenSeq             delegationSeq;
        uint32_t             delegationValidForTime;
        uint16_t             delegationFlags;
        int64_t              delegationIssuedTime;
        kfsUid_t             delegationUser;
    };

    ChunkLeases(time_t now);

    inline const WriteLease* GetWriteLease(
        const EntryKey& key) const;
    inline const WriteLease* GetChunkWriteLease(
        chunkId_t chunkId) const;
    inline const WriteLease* GetValidWriteLease(
        const EntryKey& key) const;
    inline const WriteLease* RenewValidWriteLease(
        const EntryKey&     key,
        const MetaAllocate& req);
    inline bool HasValidWriteLease(
        const EntryKey& key) const;
    inline bool HasValidLease(
        const EntryKey& key) const;
    inline bool HasWriteLease(
        const EntryKey& key) const;
    inline bool HasLease(
        const EntryKey& key) const;
    inline int ReplicaLost(
        chunkId_t          chunkId,
        const ChunkServer* chunkServer);
    inline bool NewReadLease(
        fid_t           fid,
        const EntryKey& key,
        time_t          expires,
        LeaseId&        leaseId);
    inline bool NewWriteLease(
        MetaAllocate& req);
    inline bool DeleteWriteLease(
        fid_t           fid,
        const EntryKey& key,
        LeaseId         leaseId);
    inline int Renew(
        fid_t            fid,
        const EntryKey&  key,
        LeaseId          leaseId,
        bool             allocDoneFlag = false,
        const MetaFattr* fattr         = 0,
        MetaLeaseRenew*  req           = 0);
    inline bool Delete(
        fid_t           fid,
        const EntryKey& key);
    inline bool ExpiredCleanup(
        const EntryKey& key,
        time_t          now,
        int             ownerDownExpireDelay,
        ARAChunkCache&  arac,
        CSMap&          csmap);
    inline const char* FlushWriteLease(
        const EntryKey& key,
        ARAChunkCache&  arac,
        CSMap&          csmap);
    inline void Timer(
        time_t         now,
        int            ownerDownExpireDelay,
        ARAChunkCache& arac,
        CSMap&         csmap,
        int            maxDelete);
    inline int LeaseRelinquish(
        const MetaLeaseRelinquish& req,
        ARAChunkCache&             arac,
        CSMap&                     csmap);
    inline void GetOpenFiles(
        MetaOpenFiles::ReadInfo&  openForRead,
        MetaOpenFiles::WriteInfo& openForWrite,
        const CSMap&              csmap) const;
    inline void ServerDown(
        const ChunkServerPtr& chunkServer,
        ARAChunkCache&        arac,
        CSMap&                csmap);
    inline bool UpdateReadLeaseReplicationCheck(
        chunkId_t chunkId,
        bool      setScheduleReplicationCheckFlag);
    inline bool IsReadLease(
        LeaseId leaseId);
    inline void ChangeFileId(
        chunkId_t chunkId,
        fid_t     fidFrom,
        fid_t     fidTo);
    inline void ScheduleDumpsterCleanup(
        const MetaFattr& inFa,
        const string&    inName);
    inline bool MoveFromDumpster(
        const MetaFattr& inFa,
        const string&    inName);
    void SetDumpsterCleanupDelaySec(
        int inDelay)
    {
        mDumpsterCleanupDelaySec = max(0, min(inDelay, LEASE_INTERVAL_SECS));
    }
    int GetDumpsterCleanupDelaySec() const
        { return mDumpsterCleanupDelaySec; }
    bool IsEmpty() const
        { return (mReadLeases.IsEmpty() && mWriteLeases.IsEmpty()); }
    inline void StopServicing(
        ARAChunkCache& arac,
        CSMap&         csmap);
    inline void SetTimerNextRunTime();
    inline void ProcessPendingDelete(
        int maxDelete);
    inline bool IsDeleteScheduled(
        fid_t fid) const;
    inline bool IsDeletePending(
        fid_t fid) const;
    inline int Handle(
        MetaRemoveFromDumpster& op,
        int                     maxInFlightEntriesCount);
    inline void RescheduleDumpsterCleanup(
        time_t nextRunTime)
        { mDumpsterCleanupTimer.SetNextRunTime(nextRunTime); }
    inline void Start(MetaRename& req);
    inline void Done(MetaRename& req);
private:
    class EntryKeyHash
    {
    public:
        static size_t Hash(
            const EntryKey& inVal)
            { return size_t(inVal.first); }
    };
    class RLEntry : public ReadLease
    {
    public:
        typedef LeaseId                Key;
        typedef RLEntry                Val;
        typedef QCDLListOp<RLEntry, 0> List;

        RLEntry(
            const LeaseId&   /* leaseId */,
            const ReadLease& lease)
            : ReadLease(lease)
            { List::Init(*this); }
        RLEntry(
            const ReadLease& lease)
            : ReadLease(lease)
            { List::Init(*this); }
        RLEntry(
            const RLEntry& e)
            : ReadLease(e)
            { List::Init(*this); }
        ~RLEntry()
            { List::Remove(*this); }
        const Key& GetKey() const
            { return leaseId; }
        Val& GetVal()
            { return *this; }
        const Val& GetVal() const
            { return *this; }
        Val& Get()
            { return *this; }
        const Val& Get() const
            { return *this; }
    private:
        RLEntry* mPrevPtr[1];
        RLEntry* mNextPtr[1];
        friend class QCDLListOp<RLEntry, 0>;

        RLEntry& operator=(const RLEntry&);
    };
    template<typename T>
    static void PutInExpirationListT(
        time_t expires,
        T&     node,
        T&     head)
    {
        for (T* n = &head; ;) {
            T& c = T::List::GetPrev(*n);
            if (&c == &head || c.Get().GetExpiration() <= expires) {
                T::List::Insert(node, c);
                return;
            }
            n = &c;
        }
    }
    typedef LinearHash<
        RLEntry,
        KeyCompare<RLEntry::Key>,
        StBufferT<SingleLinkedList<RLEntry>*, 4>,
        StdFastAllocator<RLEntry>
    > ChunkReadLeases;
    struct ChunkReadLeasesHead
    {
        ChunkReadLeasesHead()
            : mLeases(),
              mExpirationList(ReadLease()),
              mScheduleReplicationCheckFlag(false)
            {}
        ChunkReadLeasesHead(
            const ChunkReadLeasesHead& h)
            : mLeases(),
              mExpirationList(ReadLease()),
              mScheduleReplicationCheckFlag(h.mScheduleReplicationCheckFlag)
        {
            if (! h.mLeases.IsEmpty() ||
                    RLEntry::List::IsInList(h.mExpirationList)) {
                panic("ChunkReadLeasesHead: invalid constructor invocation");
            }
        }
        time_t GetExpiration() const
            { return RLEntry::List::GetNext(mExpirationList).expires; }
        void PutInExpirationList(
            RLEntry& entry)
        {
            PutInExpirationListT(entry.expires, entry, mExpirationList);
        }
        ChunkReadLeases mLeases;
        RLEntry         mExpirationList;
        bool            mScheduleReplicationCheckFlag;
    private:
        ChunkReadLeasesHead& operator=(
            const ChunkReadLeasesHead&);
    };
    template<typename KeyT, typename ValT>
    class EntryT
    {
    public:
        typedef EntryT<KeyT, ValT>   Entry;
        typedef QCDLListOp<Entry, 0> List;
        typedef KeyT                 Key;
        typedef Entry                Val;

        EntryT(
            const Key& inKey,
            const Val& inVal)
            : mKey(inKey),
              mVal(inVal.mVal)
            { List::Init(*this); }
        EntryT(
            const Key&  inKey,
            const ValT& inVal)
            : mKey(inKey),
              mVal(inVal)
            { List::Init(*this); }
        EntryT(
            const Entry& e)
            : mKey(e.mKey),
              mVal(e.mVal)
            { List::Init(*this); }
        EntryT()
            : mKey(),
              mVal()
            { List::Init(*this); }
        ~EntryT()
            { List::Remove(*this); }
        const Key& GetKey() const
            { return mKey; }
        const Val& GetVal() const
            { return *this; }
        Val& GetVal()
            { return *this; }
        operator ValT&()
            { return mVal; }
        operator const ValT&() const
            { return mVal; }
        ValT& Get()
            { return mVal; }
        const ValT& Get() const
            { return mVal; }

    private:
        Key  const mKey;
        ValT       mVal;
        Entry*     mPrevPtr[1];
        Entry*     mNextPtr[1];
        friend class QCDLListOp<Entry, 0>;

        EntryT& operator=(const EntryT&);
    };
    typedef EntryT<EntryKey, ChunkReadLeasesHead> REntry;
    typedef LinearHash <
        REntry,
        KeyCompare<REntry::Key, EntryKeyHash>,
        DynamicArray<SingleLinkedList<REntry>*, 13>,
        StdFastAllocator<REntry>
    > ReadLeases;
    typedef EntryT<EntryKey, WriteLease> WEntry;
    typedef LinearHash <
        WEntry,
        KeyCompare<WEntry::Key, EntryKeyHash>,
        DynamicArray<SingleLinkedList<WEntry>*, 13>,
        StdFastAllocator<WEntry>
    > WriteLeases;
    typedef TimerWheel<
        REntry,
        REntry::List,
        time_t,
        (LEASE_INTERVAL_SECS + kLeaseTimerResolutionSec) /
            kLeaseTimerResolutionSec,
        kLeaseTimerResolutionSec
    > ReadLeaseTimer;
    typedef TimerWheel<
        WEntry,
        WEntry::List,
        time_t,
        (LEASE_INTERVAL_SECS + kLeaseTimerResolutionSec) /
            kLeaseTimerResolutionSec,
        kLeaseTimerResolutionSec
    > WriteLeaseTimer;
    class FileEntry
    {
    public:
        FileEntry(
            const string& inName = string())
            : mCount(1),
              mFa(0),
              mName(inName)
            {}
        size_t           mCount;
        const MetaFattr* mFa;
        string           mName;
    };
    typedef EntryT<fid_t, FileEntry> FEntry;
    typedef LinearHash<
        FEntry,
        KeyCompare<FEntry::Key>,
        DynamicArray<SingleLinkedList<FEntry>*, 13>,
        StdFastAllocator<FEntry>
    > FileLeases;
    typedef TimerWheel<
        FEntry,
        FEntry::List,
        time_t,
        (LEASE_INTERVAL_SECS + kLeaseTimerResolutionSec) /
            kLeaseTimerResolutionSec,
        kLeaseTimerResolutionSec
    > DumpsterCleanupTimer;

    class OpenFileLister;
    friend class OpenFileLister;
    class LeaseCleanup;
    friend class LeaseCleanup;

    ReadLeases           mReadLeases;
    WriteLeases          mWriteLeases;
    FileLeases           mFileLeases;
    bool                 mTimerRunningFlag;
    ReadLeaseTimer       mReadLeaseTimer;
    WriteLeaseTimer      mWriteLeaseTimer;
    DumpsterCleanupTimer mDumpsterCleanupTimer;
    FEntry               mPendingDeleteList;
    WEntry               mWAllocationInFlightList;
    int                  mDumpsterCleanupDelaySec;
    int                  mRemoveFromDumpsterInFlightCount;

    inline LeaseId NewReadLeaseId();
    void PutInExpirationList(
        WEntry& entry)
    {
        if (entry.Get().allocInFlight) {
            WEntry::List::Insert(entry, mWAllocationInFlightList);
        } else {
            mWriteLeaseTimer.Schedule(entry, entry.Get().expires);
        }
    }
    inline void Renew(
        WEntry& wl,
        time_t  now);
    inline void Expire(
        WEntry& wl,
        time_t  now);
    inline bool ExpiredCleanup(
        REntry& rl,
        time_t  now,
        CSMap&  csmap);
    inline bool ExpiredCleanup(
        WEntry&        wl,
        time_t         now,
        int            ownerDownExpireDelay,
        ARAChunkCache& arac,
        CSMap&         csmap);
    inline int ReplicaLost(
        WEntry&            wl,
        const ChunkServer* chunkServer);
    inline void Erase(
        WEntry& wl,
        fid_t   fid);
    inline void Erase(
        REntry& readLeaseHead,
        fid_t   fid);
    inline void Erase(
        REntry& readLeaseHead,
        CSMap&  cmap);
    inline bool IsWriteLease(
        LeaseId leaseId);
    inline LeaseId NewWriteLeaseId();
    inline void DecrementFileLease(
        FEntry& entry);
    inline bool IncrementFileLease(
        FEntry& entry);
    inline void DecrementFileLease(
        fid_t fid);
    inline bool IncrementFileLease(
        fid_t fid);
    inline bool ScheduleRemoveFromDumpster(
        FEntry& entry,
        int     maxInFlightEntriesCount);
private:
    ChunkLeases(
        const ChunkLeases&);
    ChunkLeases& operator=(
        const ChunkLeases&);
};

// "Rack" (failure group) state aggregation for rack aware replica placement.
class RackInfo
{
public:
    typedef ChunkServer::RackId          RackId;
    typedef ChunkServer::StorageTierInfo StorageTierInfo;
    typedef double                       RackWeight;
    typedef CSMap::Servers               Servers;

    RackInfo()
        : mRackId(-1),
          mPossibleCandidatesCount(0),
          mRackWeight(1.0),
          mServers()
    {
        for (size_t i = 0; i < kKfsSTierCount; i++) {
            mTierCandidateCount[i] = 0;
        }
    }
    RackInfo(
        RackId                id,
        RackWeight            weight,
        const ChunkServerPtr& server)
        : mRackId(id),
          mPossibleCandidatesCount(0),
          mRackWeight(1.0),
          mServers()
    {
        RackInfo::addServer(server);
        for (size_t i = 0; i < kKfsSTierCount; i++) {
            mTierCandidateCount[i] = 0;
        }
    }
    RackInfo(
        const RackInfo& info)
        : mRackId(info.mRackId),
          mPossibleCandidatesCount(info.mPossibleCandidatesCount),
          mRackWeight(info.mRackWeight),
          mServers(info.mServers)
    {
        for (size_t i = 0; i < kKfsSTierCount; i++) {
            mTierCandidateCount[i] = info.mTierCandidateCount[i];
            mStorageTierInfo[i]    = info.mStorageTierInfo[i];
        }
    }
    RackInfo& operator=(const RackInfo& info) {
        mRackId                  = info.mRackId;
        mPossibleCandidatesCount = info.mPossibleCandidatesCount;
        mRackWeight              = info.mRackWeight;
        mServers                 = info.mServers;
        for (size_t i = 0; i < kKfsSTierCount; i++) {
            mTierCandidateCount[i] = info.mTierCandidateCount[i];
            mStorageTierInfo[i]    = info.mStorageTierInfo[i];
        }
        return *this;
    }
    RackId id() const {
        return mRackId;
    }
    void addServer(const ChunkServerPtr& server) {
        mServers.push_back(server);
    }
    bool removeServer(const ChunkServerPtr& server) {
        Servers::iterator const iter = find(
            mServers.begin(), mServers.end(), server);
        if (iter != mServers.end()) {
            mServers.erase(iter);
            return true;
        }
        return false;
    }
    const Servers& getServers() const {
        return mServers;
    }
    int getPossibleCandidatesCount() const {
        return mPossibleCandidatesCount;
    }
    int getPossibleCandidatesCount(kfsSTier_t tier) const {
        return mTierCandidateCount[tier];
    }
    void updatePossibleCandidatesCount(
            int                    delta,
            const StorageTierInfo* storageTiersDelta,
            const int*             candidatesDelta) {
        mPossibleCandidatesCount += delta;
        assert(0 <= mPossibleCandidatesCount);
        if (! storageTiersDelta) {
            return;
        }
        for (size_t i = 0; i < kKfsSTierCount; i++) {
            mStorageTierInfo[i] += storageTiersDelta[i];
            assert(mStorageTierInfo[i].IsNonNegative());
            if (candidatesDelta) {
                mTierCandidateCount[i] += candidatesDelta[i];
                assert(0 <= mTierCandidateCount[i]);
            }
        }
    }
    RackWeight getWeight() const {
        return mRackWeight;
    }
    void setWeight(RackWeight weight) {
        mRackWeight = weight;
    }
    int64_t getWeightedPossibleCandidatesCount() const {
        return (int64_t)(mRackWeight * mPossibleCandidatesCount);
    }
    int64_t getWeightedPossibleCandidatesCount(kfsSTier_t tier) const {
        return (int64_t)(mRackWeight * mTierCandidateCount[tier]);
    }
    const StorageTierInfo* getStorageTiersInfo() const {
        return mStorageTierInfo;
    }
private:
    RackId          mRackId;
    int             mPossibleCandidatesCount;
    RackWeight      mRackWeight;
    Servers         mServers;
    int             mTierCandidateCount[kKfsSTierCount];
    StorageTierInfo mStorageTierInfo[kKfsSTierCount];
};

// Atomic record append (write append) chunk allocation cache.
// The cache includes completed and in-flight chunk allocation requests.
// The cache has single entry per file.
// The cache is used to "allocate" chunks on behalf of multiple clients, and
// then "broadcast" the result of the allocation.
class ARAChunkCache
{
public:
    typedef FAPermissions  Permissions;
    typedef CSMap::Servers Servers;
    struct Entry {
        Entry(
            chunkId_t           cid   = -1,
            seq_t               cv    = -1,
            chunkOff_t          co    = -1,
            time_t              now   = 0,
            MetaAllocate*       req   = 0,
            const Permissions*  perms = 0)
            : chunkId(cid),
              chunkVersion(cv),
              offset(co),
              lastAccessedTime(now),
              lastDecayTime(now),
              spaceReservationSize(0),
              numAppendersInChunk(0),
              permissions(perms),
              servers(req ? req->servers : Servers()),
              issuedTime(req ? req->issuedTime : time_t(0)),
              authUid(req ? req->authUid : kKfsUserNone),
              lastPendingRequest(req),
              responseStr(),
              responseAccessStr(),
              shortRpcFormatFlag(false)
            {}
        bool AddPending(MetaAllocate& req);
        bool IsAllocationPending() const {
            return (lastPendingRequest != 0);
        }
        void SetResponseAccess(const MetaAllocate& req) {
            responseAccessStr = req.responseAccessStr;
            authUid           = req.authUid;
            issuedTime        = req.issuedTime;
        }
        // index into chunk->server map to work out where the block lives
        chunkId_t          chunkId;
        seq_t              chunkVersion;
        // the file offset corresponding to the last chunk
        chunkOff_t         offset;
        // when was this info last accessed; use this to cleanup
        time_t             lastAccessedTime;
        time_t             lastDecayTime;
        // chunk space reservation approximation
        int                spaceReservationSize;
        // # of appenders to which this chunk was used for allocation
        int                numAppendersInChunk;
        const Permissions* permissions;
        Servers            servers;
    private:
        time_t        issuedTime;
        kfsUid_t      authUid;
        MetaAllocate* lastPendingRequest;
        string        responseStr;
        string        responseAccessStr;
        bool          shortRpcFormatFlag;
        friend class ARAChunkCache;
    };

    ARAChunkCache()
        : mMap(),
          mTmpClear()
        {}
    ~ARAChunkCache()
        { mMap.Clear(); }
    void RequestNew(MetaAllocate& req);
    void RequestDone(const MetaAllocate& req);
    void Timeout(time_t now);
    inline bool Invalidate(fid_t fid);
    inline bool Invalidate(fid_t fid, chunkId_t chunkId);

    const Entry* Get(fid_t fid) const {
        return mMap.Find(fid);
    }
    Entry* Get(fid_t fid) {
        return mMap.Find(fid);
    }
    size_t GetSize() const {
        return mMap.GetSize();
    }
private:
    typedef vector<fid_t> TmpClear;
    typedef KVPair<fid_t, Entry> KVEntry;
    typedef LinearHash<
        KVEntry,
        KeyCompare<KVEntry::Key>,
        DynamicArray<
            SingleLinkedList<KVEntry>*,
            9 // 2^9
        >,
        StdFastAllocator<KVEntry>
    > Map;
    Map      mMap;
    TmpClear mTmpClear;
private:
    ARAChunkCache(const ARAChunkCache&);
    ARAChunkCache& operator=(const ARAChunkCache&);
};

// Run operation on a timer.
template<typename OPTYPE>
class PeriodicOp : public KfsCallbackObj, public ITimeout
{
public:
    PeriodicOp(int intervalMs)
        : mInProgress(false),
          mCmdIntervalMs(intervalMs),
          mOp(1, this) {
        SET_HANDLER(this, &PeriodicOp::HandleEvent);
        SetTimeoutInterval(mCmdIntervalMs);
        globalNetManager().RegisterTimeoutHandler(this);
    }
    virtual ~PeriodicOp() {
        assert(! mInProgress || ! globalNetManager().IsRunning());
        globalNetManager().UnRegisterTimeoutHandler(this);
    }
    void SetTimeoutInterval(int ms) {
        mCmdIntervalMs = ms;
        ITimeout::SetTimeoutInterval(mCmdIntervalMs);
    }
    int GetTimeoutInterval() const {
        return mCmdIntervalMs;
    }
    int HandleEvent(int code, void *data) {
        if (! mInProgress || EVENT_CMD_DONE != code || &mOp != data) {
            panic("invalid periodic op completion");
        }
        mInProgress = false;
        return 0;
    }
    virtual void Timeout() {
        if (mInProgress) {
            return;
        }
        mOp.opSeqno++;
        mInProgress = true;
        ITimeout::SetTimeoutInterval(mCmdIntervalMs);
        mOp.status = 0;
        mOp.statusMsg.clear();
        submit_request(&mOp);
    }
    OPTYPE& GetOp() {
        return mOp;
    }
    void ScheduleNext(int ms = 0) {
        const int intervalMs = max(0, ms);
        if (mCmdIntervalMs < intervalMs) {
            return;
        }
        ITimeout::SetTimeoutInterval(intervalMs);
        if (intervalMs <= 0) {
            globalNetManager().Wakeup();
        }
    }
private:
    /// If op is in progress, skip a send
    bool mInProgress;
    int  mCmdIntervalMs;
    /// The op for checking
    OPTYPE mOp;
private:
    PeriodicOp(const PeriodicOp&);
    PeriodicOp& operator=(const PeriodicOp&);
};

// Chunk recovery state information.
struct ChunkRecoveryInfo
{
ChunkRecoveryInfo()
    : offset(-1),
      version(-1),
      striperType(KFS_STRIPED_FILE_TYPE_NONE),
      numStripes(0),
      numRecoveryStripes(0),
      stripeSize(0),
      fileSize(-1)
    {}
    bool HasRecovery() const
        { return (numRecoveryStripes > 0); }
    void Clear()
    {
        striperType        = KFS_STRIPED_FILE_TYPE_NONE;
        offset             = -1;
        version            = -1;
        numStripes         = 0;
        numRecoveryStripes = 0;
        stripeSize         = 0;
        fileSize           = -1;
    }

    chunkOff_t offset;
    seq_t      version;
    int16_t    striperType;
    int16_t    numStripes;
    int16_t    numRecoveryStripes;
    int32_t    stripeSize;
    chunkOff_t fileSize;
};

///
/// LayoutManager is responsible for chunk allocation, re-replication, recovery,
/// and space re-balancing.
///
/// Allocating space for a chunk is a 3-way communication:
///  1. Client sends a request to the meta server for
/// allocation
///  2. Meta server picks a chunkserver to hold the chunk and
/// then sends an RPC to that chunkserver to create a chunk.
///  3. The chunkserver creates a chunk and replies to the
/// meta server's RPC.
///  4. Finally, the metaserver logs the allocation request
/// and then replies to the client.
///
/// In this model, the layout manager picks the chunkserver
/// location and queues the RPC to the chunkserver.
///
class LayoutManager : public ITimeout
{
protected:
    LayoutManager();
    virtual ~LayoutManager();

public:
    typedef CSMap::Servers               Servers;
    typedef RackInfo::RackId             RackId;
    typedef ChunkServer::StorageTierInfo StorageTierInfo;
    typedef DelegationToken::TokenSeq    TokenSeq;

    // integrate for OpenEC
    map<std::string, std::string> blk2pathname;
    void mapBlk2Path(std::string blkname, std::string pathname);
    std::string getPath4Blk(std::string blkname);
  
    vector<std::string> corruptPaths;

    static LayoutManager& Instance();

    void Shutdown();

    /// A new chunk server has joined and sent a HELLO message.
    /// Use it to configure information about that server
    /// @param[in] r  The MetaHello request sent by the
    /// new chunk server.
    void AddNewServer(MetaHello& req);

    bool Validate(MetaRetireChunkserver& req);

    /// A server is being taken down: if downtime is > 0, it is a
    /// value in seconds that specifies the time interval within
    /// which the server will connect back.  If it doesn't connect
    /// within that interval, the server is assumed to be down and
    /// re-replication will start.
    void RetireServer(MetaRetireChunkserver& req);

    /// Allocate space to hold a chunk on some
    /// chunkserver.
    /// @param[in] r The request associated with the
    /// write-allocation call.
    /// @retval 0 on success; -1 on failure
    int AllocateChunk(MetaAllocate& req,
        const vector<MetaChunkInfo*>& chunkBlock);
    int AllocateChunkFromOEC(MetaAllocate& req,
        const vector<MetaChunkInfo*>& chunkBlock,
        bool oecallocate);

    bool IsAllocationAllowed(MetaAllocate& req);

    /// When allocating a chunk for append, we try to re-use an
    /// existing chunk for a which a valid write lease exists.
    /// @param[in/out] r The request associated with the
    /// write-allocation call.  When an existing chunk is re-used,
    /// the chunkid/version is returned back to the caller.
    /// @retval 0 on success; -1 on failure
    int AllocateChunkForAppend(MetaAllocate& r);

    int ChangeChunkFid(MetaFattr* srcFattr, MetaFattr* dstFattr,
        MetaChunkInfo* chunk);

    /// A chunkid has been previously allocated.  The caller
    /// is trying to grab the write lease on the chunk. If a valid
    /// lease exists, we return it; otherwise, we assign a new lease,
    /// bump the version # for the chunk and notify the caller.
    ///
    /// @param[in] r The request associated with the
    /// write-allocation call.
    /// @retval status code
    void GetChunkWriteLease(MetaAllocate& r);

    /// Delete a chunk on the server that holds it.
    /// @param[in] chunkId The id of the chunk being deleted
    void DeleteChunk(CSMap::Entry& entry);
    void DeleteChunk(MetaAllocate& req);
    bool InvalidateAllChunkReplicas(fid_t fid, chunkOff_t offset,
        chunkId_t chunkId, seq_t& chunkVersion);

    /// A chunkserver is notifying us that a chunk it has is
    /// corrupt; so update our tables to reflect that the chunk isn't
    /// hosted on that chunkserver any more; re-replication will take
    /// care of recovering that chunk.
    /// @param[in] r  The request that describes the corrupted chunk
    void Handle(MetaChunkCorrupt& r);
    void ChunkCorrupt(chunkId_t chunkId, const ChunkServerPtr& server,
        bool notifyStale = true);
    void Handle(MetaChunkEvacuate& req);
    void Start(MetaChunkAvailable& req);
    void Handle(MetaChunkAvailable& req);
    /// Handlers to acquire and renew leases.  Unexpired leases
    /// will typically be renewed.
    void Handle(MetaLeaseAcquire& req);
    void GetChunkReadLeases(MetaLeaseAcquire& req);
    void Handle(MetaLeaseRenew& req);

    /// Handler to let a lease owner relinquish a lease.
    void Handle(MetaLeaseRelinquish& req);

    bool Validate(MetaAllocate& req);
    void CommitOrRollBackChunkVersion(MetaAllocate& req);
    void CommitOrRollBackChunkVersion(MetaLogChunkAllocate& req);

    void MakeChunkStableInit(
        const CSMap::Entry& entry,
        seq_t               chunkVersion,
        string              pathname,
        bool                beginMakeStableFlag,
        chunkOff_t          chunkSize,
        bool                hasChunkChecksum,
        uint32_t            chunkChecksum,
        bool                stripedFileFlag,
        bool                appendFlag,
        bool                leaseRelinquishFlag);
    bool AddServerToMakeStable(
        CSMap::Entry&  placementInfo,
        ChunkServerPtr server,
        chunkId_t      chunkId,
        seq_t          chunkVersion,
        const char*&   errMsg);
    void BeginMakeChunkStableDone(const MetaBeginMakeChunkStable& req);
    void LogMakeChunkStableDone(MetaLogMakeChunkStable& req);
    void MakeChunkStableDone(const MetaChunkMakeStable& req);
    void Handle(MetaLogMakeChunkStableDone& req);
    void ReplayPendingMakeStable(
        chunkId_t  chunkId,
        seq_t      chunkVersion,
        chunkOff_t chunkSize,
        bool       hasChunkChecksum,
        uint32_t   chunkChecksum,
        bool       addFlag);
    bool RestoreBeginChangeChunkVersion(
        fid_t      fid,
        chunkId_t  chunkId,
        seq_t      chunkVersion)
    {
        const bool    kPanicOnInvaliVersionFlag = false;
        string* const kStatusMsg                = 0;
        return (0 == ProcessBeginChangeChunkVersion(
            fid, chunkId, chunkVersion, kStatusMsg, kPanicOnInvaliVersionFlag));
    }
    int ProcessBeginChangeChunkVersion(
        fid_t      fid,
        chunkId_t  chunkId,
        seq_t      chunkVersion,
        string*    statusMsg,
        bool       panicOnInvaliVersionFlag);
    int WritePendingChunkVersionChange(ostream& os);
    int WritePendingMakeStable(ostream& os);
    void CancelPendingMakeStable(fid_t fid, chunkId_t chunkId);
    bool Start(MetaChunkSize& req);
    void Handle(MetaChunkSize& req);
    bool IsChunkStable(chunkId_t chunkId) const;
    const char* AddNotStableChunk(
        const ChunkServerPtr& server,
        chunkId_t             chunkId,
        seq_t                 chunkVersion,
        bool                  appendFlag,
        const ServerLocation& logPrefix);
    void ProcessPendingBeginMakeStable();

    /// Add a mapping from chunkId -> server.
    /// @param[in] chunkId  chunkId that has been stored
    /// on server c
    /// @param[in] fid  fileId associated with this chunk.
    MetaChunkInfo* AddChunkToServerMapping(MetaFattr* fattr,
        chunkOff_t offset, chunkId_t chunkId, seq_t chunkVersion,
        bool& newEntryFlag);

    /// Get the mapping from chunkId -> server.
    /// @param[in] chunkId  chunkId that has been stored
    /// on some server(s)
    /// @param[out] c   server(s) that stores chunk chunkId
    /// @retval 0 if a mapping was found; -1 otherwise
    ///
    int GetChunkToServerMapping(MetaChunkInfo& chunkInfo, Servers &c,
        MetaFattr*& fa, bool* orderReplicasFlag = 0);

    /// Get the mapping from chunkId -> file id.
    /// @param[in] chunkId  chunkId
    /// @param[out] fileId  file id the chunk belongs to
    /// @retval true if a mapping was found; false otherwise
    ///
    bool GetChunkFileId(chunkId_t chunkId, fid_t& fileId,
        const MetaChunkInfo** chunkInfo = 0, const MetaFattr** fa = 0,
        LayoutManager::Servers* srvs = 0);

    /// Dump out the chunk location map to a file.  The file is
    /// written to the specified dir.  The filename:
    /// <dir>/chunkmap.txt.<pid>
    ///
    void DumpChunkToServerMap(const string &dir);

    /// Dump out the chunk location map to a string stream.
    void DumpChunkToServerMap(ostream &os);

    /// Dump out the list of chunks that are currently replication
    /// candidates.
    void Handle(MetaDumpChunkReplicationCandidates& req);

    /// Check the replication level of all the blocks and report
    /// back files that are under-replicated.
    /// Returns true if the system is healthy.
    int  FsckStreamCount(bool reportAbandonedFilesFlag) const;
    bool Fsck(ostream** os, bool reportAbandonedFilesFlag);

    /// For monitoring purposes, dump out state of all the
    /// connected chunk servers.
    void Handle(MetaPing& inReq, bool wormModeFlag);

    /// Return a list of alive chunk servers
    void UpServers(ostream &os);

    /// Periodically, walk the table of chunk -> [location, lease]
    /// and remove out dead leases.
    void LeaseCleanup(int64_t startTime);

    /// Periodically, re-check the replication level of all chunks
    /// the system; this call initiates the checking work, which
    /// gets done over time.
    void InitCheckAllChunks();

    /// Is an expensive call; use sparingly
    void CheckAllLeases();

    /// Cleanup the lease for a particular chunk
    /// @param[in] chunkId  the chunk for which leases need to be cleaned up
    /// @param[in] v   the placement/lease info for the chunk
    void LeaseCleanup(chunkId_t chunkId, CSMap::Entry &v);
    bool ExpiredLeaseCleanup(int64_t now, chunkId_t chunkId);

    /// Handler that loops thru the chunk->location map and determines
    /// if there are sufficient copies of each chunk.  Those chunks with
    /// fewer copies are (re) replicated.
    void ChunkReplicationChecker();

    /// A set of nodes have been put in hibernation by an admin.
    /// This is done for scheduled downtime.  During this period, we
    /// don't want to pro-actively replicate data on the down nodes;
    /// if the node doesn't come back as promised, we then start
    /// re-replication.  Periodically, check the status of
    /// hibernating nodes.
    void CheckHibernatingServersStatus();

    /// A chunk replication operation finished.  If the op was successful,
    /// then, we update the chunk->location map to record the presence
    /// of a new replica.
    /// @param[in] req  The op that we sent to a chunk server asking
    /// it to do the replication.
    void Handle(MetaChunkReplicate& req);

    /// Degree of replication for chunk has changed.  When the replication
    /// checker runs, have it check the status for this chunk.
    /// @param[in] chunkId  chunk whose replication level needs checking
    ///
    void ChangeChunkReplication(chunkId_t chunkId);

    /// Get all the fid's for which there is an open lease (read/write).
    /// This is useful for reporting purposes.
    /// @param[out] openForRead, openForWrite: the pathnames of files
    /// that are open for reading/writing respectively
    void GetOpenFiles(
        MetaOpenFiles::ReadInfo&  openForRead,
        MetaOpenFiles::WriteInfo& openForWrite);

    void InitRecoveryStartTime() {
        mRecoveryStartTime = time(0);
    }

    void SetMinChunkserversToExitRecovery(uint32_t n) {
        mMinChunkserversToExitRecovery = n;
    }

    void ToggleRebalancing(bool v) {
        mIsRebalancingEnabled = v;
    }

    /// Methods for doing "planned" rebalancing of data.
    /// Read in the file that lays out the plan
    /// Return 0 if we can open the file; -1 otherwise
    int LoadRebalancePlan(const string& planFn);

    /// Execute the plan for all servers
    void ExecuteRebalancePlan();

    /// Execute planned rebalance for server c
    size_t ExecuteRebalancePlan(
        const ChunkServerPtr& c,
        bool&                 serverDownFlag,
        int&                  maxScan,
        int64_t               maxTime,
        int&                  nextTimeCheck);

    bool SetParameters(const Properties& props, int clientPort = -1);
    void SetChunkServersProperties(const Properties& props);

    void GetChunkServerCounters(IOBuffer& buf);
    void GetChunkServerDirCounters(IOBuffer& buf);

    void AllocateChunkForAppendDone(MetaAllocate& req) {
        mARAChunkCache.RequestDone(req);
    }

    uint32_t GetConcurrentWritesPerNodeWatermark() const {
        return mConcurrentWritesPerNodeWatermark;
    }
    double GetMaxSpaceUtilizationThreshold() const {
        return mMaxSpaceUtilizationThreshold;
    }
    int GetInFlightChunkOpsCount(chunkId_t chunkId, MetaOp opType,
        chunkOff_t objStoreBlockPos = -1) const;
    int GetInFlightChunkModificationOpCount(chunkId_t chunkId,
         chunkOff_t objStoreBlockPos = -1, Servers* srvs = 0) const;
    int GetInFlightChunkOpsCount(chunkId_t chunkId, const MetaOp* opTypes,
         chunkOff_t objStoreBlockPos = -1, Servers* srvs = 0) const;
    void DoCheckpoint() {
        mCheckpoint.GetOp().ScheduleNow();
        mCheckpoint.Timeout();
    }
    void SetBufferPool(QCIoBufferPool* pool)
        { mBufferPool = pool; }
    QCIoBufferPool* GetBufferPool()
        { return mBufferPool; }
    int64_t GetFreeIoBufferByteCount() const;
    void Done(MetaChunkVersChange& req);
    virtual void Timeout();
    bool Validate(MetaHello& r);
    bool CanAddServer(size_t pendingAddSize);
    void Start(MetaHello& r);
    void UpdateDelayedRecovery(const MetaFattr& fa, bool forceUpdateFlag = false);
    bool HasWriteAppendLease(chunkId_t chunkId) const;
    void ScheduleRestartChunkServers();
    void UpdateSrvLoadAvg(ChunkServer& srv, int64_t delta,
        const StorageTierInfo* stiersDelta, bool canBeCandidateFlag = true);
    int16_t GetMaxReplicasPerFile() const
        { return mMaxReplicasPerFile; }
    int16_t GetMaxReplicasPerRSFile() const
        { return mMaxReplicasPerRSFile; }
    int64_t GetMaxFsckTime() const
        { return mMaxFsckTime; }
    bool HasEnoughFreeBuffers(MetaRequest* req = 0);
    int GetMaxResponseSize() const
        { return mMaxResponseSize; }
    int GetReadDirLimit() const
        { return mReadDirLimit; }
    void ChangeIoBufPending(int64_t delta)
        { SyncAddAndFetch(mIoBufPending, delta); }
    bool IsCandidateServer(
        const ChunkServer& c,
        kfsSTier_t         tier                         = kKfsSTierUndef,
        double             writableChunksThresholdRatio = 1.0);
    bool GetPanicOnInvalidChunkFlag() const
        { return mPanicOnInvalidChunkFlag; }

    // Chunk placement.
    enum { kSlaveScaleFracBits = 8 };
    typedef vector<RackInfo, StdAllocator<RackInfo> > RackInfos;

    bool GetSortCandidatesBySpaceUtilizationFlag() const
        { return mSortCandidatesBySpaceUtilizationFlag; }
    bool GetSortCandidatesByLoadAvgFlag() const
        { return mSortCandidatesByLoadAvgFlag; }
    bool GetUseFsTotalSpaceFlag() const
        { return mUseFsTotalSpaceFlag; }
    int64_t GetSlavePlacementScale();
    int GetMaxConcurrentWriteReplicationsPerNode() const
        { return mMaxConcurrentWriteReplicationsPerNode; }
    const Servers& GetChunkServers() const
        { return mChunkServers; }
    const RackInfos& GetRacks() const
        { return mRacks; }
    int64_t Rand(int64_t interval);
    PrngIsaac64& GetRandom() { return mRandom; }
    void UpdateChunkWritesPerDrive(
        ChunkServer&           srv,
        int                    deltaNumChunkWrites,
        int                    deltaNumWritableDrives,
        const StorageTierInfo* tiersDelta);

    // Unix style permissions
    kfsUid_t GetDefaultUser() const
        { return mDefaultUser; }
    kfsGid_t GetDefaultGroup() const
        { return mDefaultGroup; }
    kfsMode_t GetDefaultFileMode() const
        { return mDefaultFileMode; }
    kfsMode_t GetDefaultDirMode() const
        { return mDefaultDirMode; }
    kfsUid_t GetDefaultLoadUser() const
        { return mDefaultLoadUser; }
    kfsGid_t GetDefaultLoadGroup() const
        { return mDefaultLoadGroup; }
    kfsMode_t GetDefaultLoadFileMode() const
        { return mDefaultLoadFileMode; }
    kfsMode_t GetDefaultLoadDirMode() const
        { return mDefaultLoadDirMode; }
    bool VerifyAllOpsPermissions() const
        { return mVerifyAllOpsPermissionsFlag; }
    void SetEUserAndEGroup(MetaRequest& req)
    {
        if (req.fromChunkServerFlag) {
            req.euser  = kKfsUserNone;
            req.egroup = kKfsGroupNone;
            return;
        }
        SetUserAndGroup(req, req.euser, req.egroup);
        if (mForceEUserToRootFlag) {
            req.euser = kKfsUserRoot;
        }
        if (req.euser != kKfsUserRoot || mRootHosts.empty()) {
            return;
        }
        if (mRootHosts.find(req.clientIp) == mRootHosts.end()) {
            req.euser = kKfsUserNone;
        }
    }
    void SetUserAndGroup(const MetaRequest& req,
        kfsUid_t& user, kfsGid_t& group)
    {
        if (mHostUserGroupRemap.empty() || req.clientIp.empty()) {
            return;
        }
        SetUserAndGroupSelf(req, user, group);
    }

    int GetTierCandidatesCount(kfsSTier_t tier) const
        { return mTierCandidatesCount[tier]; }
    double GetMaxTierSpaceUtilization(kfsSTier_t tier) const
    {
        return min(mTierSpaceUtilizationThreshold[tier],
                    mMaxSpaceUtilizationThreshold);
    }
    template<typename T> static
    T FindRackT(T first, T last, RackId id) {
        if (id < 0) {
            return last;
        }
        T const it = lower_bound(first, last, id, bind(&RackInfo::id, _1) < id);
        return ((it == last || it->id() != id) ? last : it);
    }
    uint64_t GetAuthCtxUpdateCount() const
        { return mAuthCtxUpdateCount; }
    bool UpdateClientAuthContext(
        uint64_t& authCtxUpdateCount, AuthContext& authCtx)
    {
        if (authCtxUpdateCount == mAuthCtxUpdateCount) {
            return false;
        }
        UpdateClientAuth(authCtx);
        authCtxUpdateCount = mAuthCtxUpdateCount;
        return true;
    }
    AuthContext& GetClientAuthContext()
        { return mClientAuthContext; }
    AuthContext& GetCSAuthContext()
        { return mCSAuthContext; }
    const Properties& GetConfigParameters() const
        { return mConfigParameters; }
    bool IsClientCSAuthRequired() const
        { return mClientCSAuthRequiredFlag; }
    bool IsClientCSAllowClearText() const
        { return mClientCSAllowClearTextFlag; }
    int GetCSAccessValidForTime() const
        { return mCSAccessValidForTimeSec; }
    const UserAndGroup& GetUserAndGroup() const
        { return mUserAndGroup; }
    UserAndGroup& GetUserAndGroup()
        { return mUserAndGroup; }
    bool HasMetaServerAdminAccess(MetaRequest& op)
    {
        return (
            ! IsVerificationOfStatsOrAdminPermissionsRequired(op) ||
            mUserAndGroup.IsMetaServerAdminUser(op.euser)
        );
    }
    bool HasMetaServerStatsAccess(MetaRequest& op)
    {
        return (
            ! IsVerificationOfStatsOrAdminPermissionsRequired(op) ||
            mUserAndGroup.IsMetaServerStatsUser(op.euser)
        );
    }
    bool IsFileSystemIdRequired() const
        { return mFileSystemIdRequiredFlag; }
    bool IsDeleteChunkOnFsIdMismatch() const
        { return mDeleteChunkOnFsIdMismatchFlag; }
    void Handle(MetaForceChunkReplication& op);
    bool Validate(MetaCreate& createOp) const;
    IdempotentRequestTracker& GetIdempotentRequestTracker()
        { return mIdempotentRequestTracker; }
    void ScheduleDumpsterCleanup(const MetaFattr& fa, const string& name);
    void Handle(MetaRemoveFromDumpster& op);
    void SetPrimary(bool flag);
    bool IsPrimary() const
        { return mPrimaryFlag; }
    const bool& GetPrimaryFlag() const
        { return mPrimaryFlag; }
    void ChangeChunkVersion(chunkId_t chunkId, seq_t version,
        MetaAllocate* req);
    void SetChunkVersion(MetaChunkInfo& chunkInfo, seq_t version);
    bool IsObjectStoreEnabled() const
        { return mObjectStoreEnabledFlag; }
    bool GetAccessProxy(
        const MetaRequest& req,
        Servers&           servers);
    void Done(MetaChunkDelete& req);
    void DeleteFile(const MetaFattr& fa);
    int  WritePendingObjStoreDelete(ostream& os);
    bool AddPendingObjStoreDelete(
        chunkId_t chunkId, chunkOff_t first, chunkOff_t last);
    void ClearObjStoreDelete();
    bool IsObjectStoreDeleteEmpty() const;
    void Handle(MetaLogClearObjStoreDelete& req);
    void UpdateObjectsCount(
        ChunkServer& srv, int64_t delta, int64_t writableDelta);
    void Handle(MetaChunkLogInFlight& req);
    void Start(MetaBye& req);
    void Handle(MetaBye& req);
    void Handle(MetaChunkLogCompletion& req);
    void Handle(MetaHibernatedPrune& req);
    void Handle(MetaHibernatedRemove& req);
    int WriteChunkServers(ostream& os) const;
    bool RestoreStart();
    bool RestoreChunkServer(const ServerLocation& loc,
        size_t idx, size_t chunks, const CIdChecksum& chksum,
        bool retiringFlag, int64_t retStart, int64_t retDown,
        bool retiredFlag, RackId rackId, bool pendingHelloNotifyFlag);
    const ChunkServerPtr& RestoreGetChunkServer() const
        { return mRestoreChunkServerPtr; }
    void RestoreClearChunkServer()
        { mRestoreChunkServerPtr.reset(); }
    bool RestoreHibernatedCS(
        const ServerLocation& loc, size_t idx,
        size_t chunks, const CIdChecksum& chksum,
        const CIdChecksum& modChksum,
        int64_t startTime, int64_t endTime, bool retiredFlag,
        bool pendingHelloNotifyFlag);
    const HibernatedChunkServerPtr& RestoreGetHibernatedCS() const
        { return mRestoreHibernatedCSPtr; }
    void RestoreClearHibernatedCS()
        { mRestoreHibernatedCSPtr.reset(); }
    ostream& Checkpoint(ostream& os, const MetaChunkInfo& info) const;
    bool Restore(MetaChunkInfo& info,
        const char* restoreIdxs, size_t restoreIdxsLen, bool hexFmtFlag);
    void StartServicing();
    void StopServicing();
    void ReplaySetRack(bool flag)
        { mReplaySetRackFlag = flag; }
    time_t TimeNow() const
        { return mNetManager.Now(); }
    time_t GetServiceStartTime() const
        { return mServiceStartTime; }
    bool IsPingResponseUpToDate() const
    {
        return (! mPingResponse.IsEmpty() &&
            TimeNow() < mPingUpdateTime + mPingUpdateInterval);
    }
    void Disconnected(const ChunkServer& srv);
    template<typename T>
    void GetPendingObjStoreDelete(T& func) const
    {
        const ObjStoreFilesDeleteQueue::Entry* fde =
            mObjStoreFilesDeleteQueue.Front();
        bool blockFlag = false;
        while (fde) {
            func(fde->mFid, fde->mLast, blockFlag);
            fde = fde->GetNext();
        }
        blockFlag = true;
        const ObjBlockDeleteQueueEntry* dre;
        ObjBlocksDeleteRequeue::ConstIterator rit(mObjBlocksDeleteRequeue);
        while ((dre = rit.Next())) {
            func(dre->first, dre->second, blockFlag);
        }
        const ObjBlocksDeleteInFlightEntry* dfe;
        ObjBlocksDeleteInFlight::ConstIterator fit(mObjBlocksDeleteInFlight);
        while ((dfe = fit.Next())) {
            func(dfe->GetVal().first, (-dfe->GetVal().second - 1), blockFlag);
        }
    }
    int RunFsck(
        const string& tmpPrefix, bool reportAbandonedFilesFlag, ostream& os);
    int RunFsck(const string& fileName, bool reportAbandonedFilesFlag);
    void Start(MetaTruncate& req);
    void Handle(MetaTruncate& req);
    void Handle(MetaSetATime& req);
    void UpdateATime(const MetaFattr* fa, MetaReaddir& req);
    void UpdateATime(const MetaFattr* fa, MetaReaddirPlus& req);
    void Start(MetaRename& req);
    void Done(MetaRename& req);
protected:
    typedef vector<
        int,
        StdAllocator<int>
    > RackIds;
    class ObjStoreFilesDeleteQueue
    {
    public:
        class Entry
        {
        public:
            static Entry*& Next(Entry& entry)
                { return entry.mNext; }
            const Entry* GetNext() const
                { return mNext; }
            const time_t mTime;
            const fid_t  mFid;
            chunkOff_t   mLast;
        private:
            Entry* mNext;
            Entry(
                time_t     time,
                fid_t      fid,
                chunkOff_t last)
                : mTime(time),
                  mFid(fid),
                  mLast(last),
                  mNext(0)
                {}
            ~Entry()
                {}
            friend class ObjStoreFilesDeleteQueue;
        };
        ObjStoreFilesDeleteQueue()
            : mQueue(),
              mSize(0),
              mAllocator()
            {}
        ~ObjStoreFilesDeleteQueue()
            { ObjStoreFilesDeleteQueue::Clear(); }
        Entry* Front()
            { return mQueue.Front(); }
        const Entry* Front() const
            { return mQueue.Front(); }
        bool IsEmpty() const
            { return mQueue.IsEmpty(); }
        void Add(time_t time, fid_t fid, chunkOff_t last)
        {
            Entry* const entry =
                new (mAllocator.Allocate()) Entry(time, fid, last);
            mQueue.PushBack(*entry);
            mSize++;
        }
        void Remove()
        {
            Entry* const entry = mQueue.PopFront();
            if (entry) {
                mSize--;
                Delete(entry);
            }
        }
        void Clear()
        {
            Queue queue;
            queue.PushBack(mQueue);
            mSize = 0;
            Entry* entry;
            while ((entry = queue.PopFront())) {
                Delete(entry);
            }
        }
        size_t GetSize() const
            { return mSize; }
    private:
        typedef PoolAllocator<
            sizeof(Entry),
            size_t(1) << 20, // size_t TMinStorageAlloc,
            size_t(8) << 20, // size_t TMaxStorageAlloc,
            true             // bool   TForceCleanupFlag
        > Allocator;
        typedef SingleLinkedQueue<Entry, Entry> Queue;
        Queue     mQueue;
        size_t    mSize;
        Allocator mAllocator;

        void Delete(Entry* entry)
        {
            entry->~Entry();
            mAllocator.Deallocate(entry);
        }
    };
    typedef pair<chunkId_t, chunkOff_t>                ObjBlockDeleteQueueEntry;
    typedef DynamicArray<ObjBlockDeleteQueueEntry,  8> ObjBlocksDeleteRequeue;
    typedef KeyOnly<
        pair<chunkId_t, seq_t>
    > ObjBlocksDeleteInFlightEntry;
    class ObjBlocksDeleteInFlightEntryHash
    {
    public:
        static size_t Hash(
            const ObjBlocksDeleteInFlightEntry::Key& inVal)
            { return size_t(inVal.first ^ inVal.second); }
    };
    typedef LinearHash <
        ObjBlocksDeleteInFlightEntry,
        KeyCompare<
            ObjBlocksDeleteInFlightEntry::Key,
            ObjBlocksDeleteInFlightEntryHash
        >,
        DynamicArray<SingleLinkedList<ObjBlocksDeleteInFlightEntry>*, 17>,
        PoolAllocatorAdapter<
            ObjBlocksDeleteInFlightEntry,
            size_t(1) << 20, // size_t TMinStorageAlloc,
            size_t(8) << 20, // size_t TMaxStorageAlloc,
            true             // bool   TForceCleanupFlag
       >
    > ObjBlocksDeleteInFlight;
    typedef SingleLinkedQueue<MetaRequest, MetaRequest::GetNext> RequestQueue;
    class RebalanceCtrs
    {
    public:
        typedef int64_t Counter;

        RebalanceCtrs()
            : mRoundCount(0),
              mNoSource(0),
              mServerNeeded(0),
              mNoServerFound(0),
              mRackNeeded(0),
              mNoRackFound(0),
              mNonLoadedServerNeeded(0),
              mNoNonLoadedServerFound(0),
              mOk(0),
              mScanned(0),
              mBusy(0),
              mBusyOther(0),
              mReplicationStarted(0),
              mNoReplicationStarted(0),
              mScanTimeout(0),
              mTotalNoSource(0),
              mTotalServerNeeded(0),
              mTotalNoServerFound(0),
              mTotalRackNeeded(0),
              mTotalNoRackFound(0),
              mTotalNonLoadedServerNeeded(0),
              mTotalNoNonLoadedServerFound(0),
              mTotalOk(0),
              mTotalScanned(0),
              mTotalBusy(0),
              mTotalBusyOther(0),
              mTotalReplicationStarted(0),
              mTotalNoReplicationStarted(0),
              mTotalScanTimeout(0),
              mPlan(0),
              mPlanNoDest(0),
              mPlanTimeout(0),
              mPlanScanned(0),
              mPlanNoChunk(0),
              mPlanNoSrc(0),
              mPlanBusy(0),
              mPlanBusyOther(0),
              mPlanCannotMove(0),
              mPlanReplicationStarted(0),
              mPlanNoReplicationStarted(0),
              mPlanLine(0),
              mPlanAdded(0),
              mPlanNoServer(0),
              mTotalPlanNoDest(0),
              mTotalPlanTimeout(0),
              mTotalPlanScanned(0),
              mTotalPlanNoChunk(0),
              mTotalPlanNoSrc(0),
              mTotalPlanBusy(0),
              mTotalPlanBusyOther(0),
              mTotalPlanCannotMove(0),
              mTotalPlanReplicationStarted(0),
              mTotalPlanNoReplicationStarted(0),
              mTotalPlanLine(0),
              mTotalPlanAdded(0),
              mTotalPlanNoServer(0)
            {}
        void Clear()
        {
            *this = RebalanceCtrs();
        }
        void NoSource()
        {
            mNoSource++;
            mTotalNoSource++;
        }
        void ServerOk()
        {
            mOk++;
            mTotalOk++;
        }
        void ServerNeeded()
        {
            mServerNeeded++;
            mTotalServerNeeded++;
        }
        void NoServerFound()
        {
            mNoServerFound++;
            mTotalNoServerFound++;
        }
        void RackNeeded()
        {
            mRackNeeded++;
            mTotalRackNeeded++;
        }
        void NoRackFound()
        {
            mNoRackFound++;
            mTotalNoRackFound++;
        }
        void NonLoadedServerNeeded()
        {
            mNonLoadedServerNeeded++;
            mTotalNonLoadedServerNeeded++;
        }
        void NoNonLoadedServerFound()
        {
            mNoNonLoadedServerFound++;
            mTotalNoNonLoadedServerFound++;
        }
        void ReplicationStarted()
        {
            mReplicationStarted++;
            mTotalReplicationStarted++;
        }
        void NoReplicationStarted()
        {
            mNoReplicationStarted++;
            mTotalNoReplicationStarted++;
        }
        void Scanned()
        {
            mScanned++;
            mTotalScanned++;
        }
        void Busy()
        {
            mBusy++;
            mTotalBusy++;
        }
        void BusyOther()
        {
            mBusyOther++;
            mTotalBusyOther++;
        }

        void ScanTimeout()
        {
            mScanTimeout++;
            mTotalScanTimeout++;
        }
        void NextRound()
        {
            mRoundCount++;
            mServerNeeded = 0;
            mNoServerFound = 0;
            mRackNeeded = 0;
            mNoRackFound = 0;
            mNonLoadedServerNeeded = 0;
            mNoNonLoadedServerFound = 0;
            mOk = 0;
            mScanned = 0;
            mBusy = 0;
            mBusyOther = 0;
            mReplicationStarted = 0;
            mNoReplicationStarted = 0;
            mScanTimeout = 0;
        }
        void StartPlan()
        {
            mPlan++;
            mPlanNoDest = 0;
            mPlanTimeout = 0;
            mPlanScanned = 0;
            mPlanNoChunk = 0;
            mPlanNoSrc = 0;
            mPlanBusy = 0;
            mPlanBusyOther = 0;
            mPlanCannotMove = 0;
            mPlanReplicationStarted = 0;
            mPlanNoReplicationStarted = 0;
            mPlanLine = 0;
            mPlanAdded = 0;
            mPlanNoServer = 0;
        }
        void PlanNoDest()
        {
            mPlanNoDest++;
            mTotalPlanNoDest++;
        }
        void PlanTimeout()
        {
            mPlanTimeout++;
            mTotalPlanTimeout++;
        }
        void PlanScanned()
        {
            mPlanScanned++;
            mTotalPlanScanned++;
        }
        void PlanNoChunk()
        {
            mPlanNoChunk++;
            mTotalPlanNoChunk++;
        }
        void PlanNoSrc()
        {
            mPlanNoSrc++;
            mTotalPlanNoSrc++;
        }
        void PlanBusy()
        {
            mPlanBusy++;
            mTotalPlanBusy++;
        }
        void PlanBusyOther()
        {
            mPlanBusyOther++;
            mTotalPlanBusyOther++;
        }
        void PlanCannotMove()
        {
            mPlanCannotMove++;
            mTotalPlanCannotMove++;
        }
        void PlanReplicationStarted()
        {
            mPlanReplicationStarted++;
            mTotalPlanReplicationStarted++;
        }
        void PlanNoReplicationStarted()
        {
            mPlanNoReplicationStarted++;
            mTotalPlanNoReplicationStarted++;
        }
        void PlanLine()
        {
            mPlanLine++;
            mTotalPlanLine++;
        }
        void PlanAdded()
        {
            mPlanAdded++;
            mTotalPlanAdded++;
        }
        void PlanNoServer()
        {
            mPlanNoServer++;
            mTotalPlanNoServer++;
        }
        Counter GetPlanLine() const
        {
            return mPlanLine;
        }
        Counter GetTotalScanned() const
        {
            return mTotalScanned;
        }
        Counter GetRoundCount() const
        {
            return mRoundCount;
        }
        ostream& Show(ostream& os,
            const char* prefix = 0, const char* suffix = 0);
    private:
        Counter mRoundCount;
        Counter mNoSource;
        Counter mServerNeeded;
        Counter mNoServerFound;
        Counter mRackNeeded;
        Counter mNoRackFound;
        Counter mNonLoadedServerNeeded;
        Counter mNoNonLoadedServerFound;
        Counter mOk;
        Counter mScanned;
        Counter mBusy;
        Counter mBusyOther;
        Counter mReplicationStarted;
        Counter mNoReplicationStarted;
        Counter mScanTimeout;
        Counter mTotalNoSource;
        Counter mTotalServerNeeded;
        Counter mTotalNoServerFound;
        Counter mTotalRackNeeded;
        Counter mTotalNoRackFound;
        Counter mTotalNonLoadedServerNeeded;
        Counter mTotalNoNonLoadedServerFound;
        Counter mTotalOk;
        Counter mTotalScanned;
        Counter mTotalBusy;
        Counter mTotalBusyOther;
        Counter mTotalReplicationStarted;
        Counter mTotalNoReplicationStarted;
        Counter mTotalScanTimeout;
        Counter mPlan;
        Counter mPlanNoDest;
        Counter mPlanTimeout;
        Counter mPlanScanned;
        Counter mPlanNoChunk;
        Counter mPlanNoSrc;

        Counter mPlanBusy;
        Counter mPlanBusyOther;
        Counter mPlanCannotMove;
        Counter mPlanReplicationStarted;
        Counter mPlanNoReplicationStarted;
        Counter mPlanLine;
        Counter mPlanAdded;
        Counter mPlanNoServer;
        Counter mTotalPlanNoDest;
        Counter mTotalPlanTimeout;
        Counter mTotalPlanScanned;
        Counter mTotalPlanNoChunk;
        Counter mTotalPlanNoSrc;
        Counter mTotalPlanBusy;
        Counter mTotalPlanBusyOther;
        Counter mTotalPlanCannotMove;
        Counter mTotalPlanReplicationStarted;
        Counter mTotalPlanNoReplicationStarted;
        Counter mTotalPlanLine;
        Counter mTotalPlanAdded;
        Counter mTotalPlanNoServer;
    };

    // Striped (Reed-Solomon) files allocations in flight used for chunk
    // placment.
    typedef set<
        pair<pair<fid_t, chunkOff_t>, chunkId_t>,
        less<pair<pair<fid_t, chunkOff_t>, chunkId_t> >,
        StdFastAllocator<pair<pair<fid_t, chunkOff_t>, chunkId_t> >
    > StripedFilesAllocationsInFlight;

    class FilesChecker;

    NetManager& mNetManager;

    /// A counter to track the # of ongoing chunk replications
    int mNumOngoingReplications;

    /// A switch to toggle rebalancing: if the system is under load,
    /// we'd like to turn off rebalancing.  We can enable it a
    /// suitable time.
    bool mIsRebalancingEnabled;

    /// For the purposes of rebalancing, what is the range we want
    /// a node to be in.  If a node is outside the range, it is
    /// either underloaded (in which case, it can take blocks) or it
    /// is overloaded (in which case, it can give up blocks).
    double mMaxRebalanceSpaceUtilThreshold;
    double mMinRebalanceSpaceUtilThreshold;

    /// Set when a rebalancing plan is being excuted.
    bool mIsExecutingRebalancePlan;

    /// After a crash, track the recovery start time.  For a timer
    /// period that equals the length of lease interval, we only grant
    /// lease renews and new leases to new chunks.  We however,
    /// disallow granting new leases to existing chunks.  This is
    /// because during the time period that corresponds to a lease interval,
    /// we may learn about leases that we had handed out before crashing.
    time_t mRecoveryStartTime;
    /// To keep track of uptime.
    const time_t mStartTime;

    /// Defaults to the width of a lease window
    int mRecoveryIntervalSec;
    int mLeaseCleanerOtherIntervalSec;
    time_t mLeaseCleanerOtherNextRunTime;

    /// Periodically clean out dead leases
    PeriodicOp<MetaLeaseCleanup> mLeaseCleaner;

    /// Similar to the lease cleaner: periodically check if there are
    /// sufficient copies of each chunk.
    PeriodicOp<MetaChunkReplicationCheck> mChunkReplicator;
    PeriodicOp<MetaCheckpoint> mCheckpoint;

    size_t mMinChunkserversToExitRecovery;

    /// List of connected chunk servers.
    Servers mChunkServers;

    //
    // For maintenance reasons, we'd like to schedule downtime for a server.
    // When the server is taken down, a promise is made---the server will go
    // down now and come back up by a specified time. During this window, we
    // are willing to tolerate reduced # of copies for a block.  Now, if the
    // server doesn't come up by the promised time, the metaserver will
    // initiate re-replication of blocks on that node.  This ability allows
    // us to schedule downtime on a node without having to incur the
    // overhead of re-replication.
    //
    struct HibernatingServerInfo
    {
        HibernatingServerInfo(
                const ServerLocation& loc       = ServerLocation(),
                time_t                startTime = time_t(),
                int64_t               downTime  = 0,
                bool                  rplFlag   = false,
                size_t                idx       = ~size_t(0),
                bool                  rtrFlag   = false)
            : location(loc),
              startTime(startTime),
              sleepEndTime((time_t)(startTime + downTime)),
              csmapIdx(idx),
              removeOp(0),
              replayFlag(rplFlag),
              retiredFlag(rtrFlag)
              {}
        bool IsHibernated() const { return (csmapIdx != ~size_t(0)) ; }
        // the server we put in hibernation
        ServerLocation        location;
        time_t                startTime;
        // when is it likely to wake up
        time_t                sleepEndTime;
        // CSMap server index to remove hibernated server.
        size_t                csmapIdx;
        MetaHibernatedRemove* removeOp;
        bool                  replayFlag;
        bool                  retiredFlag;
    };
    typedef vector<
        HibernatingServerInfo,
        StdAllocator<HibernatingServerInfo>
    > HibernatedServerInfos;
    /// List of servers that are hibernating; if they don't wake up
    /// the time the hibernation period ends, the blocks on those
    /// nodes needs to be re-replicated.  This provides us the ability
    /// to take a node down for maintenance and bring it back up
    /// without incurring re-replication overheads.
    HibernatedServerInfos mHibernatingServers;

    /// Track when servers went down so we can report it
    typedef deque<string> DownServers;
    DownServers mDownServers;

    /// State about how each rack (such as, servers/space etc)
    RackInfos mRacks;

    /// Mapping from a chunk to its location(s).
    CSMap mChunkToServerMap;

    StripedFilesAllocationsInFlight mStripedFilesAllocationsInFlight;

    /// chunks to which a lease has been handed out; whenever we
    /// cleanup the leases, this set is walked
    ChunkLeases mChunkLeases;

    /// For files that are being atomic record appended to, track the last
    /// chunk of the file that we can use for subsequent allocations
    ARAChunkCache mARAChunkCache;

    // Chunks are made stable by a message from the metaserver ->
    // chunkservers.  To prevent clients from seeing non-stable chunks, the
    // metaserver delays issuing a read lease to a client if there is a make
    // stable message in-flight.  Whenever the metaserver receives acks for
    // the make stable message, it needs to update its state. This structure
    // tracks the # of messages sent out and how many have been ack'ed.
    // When all the messages have been ack'ed the entry for a particular
    // chunk can be cleaned up.
    struct MakeChunkStableInfo
    {
        MakeChunkStableInfo(
            int    nServers        = 0,
            bool   beginMakeStable = false,
            string name            = string(),
            seq_t  cvers           = -1,
            bool   stripedFile     = false,
            bool   updateMTime     = false)
            : beginMakeStableFlag(beginMakeStable),
              logMakeChunkStableFlag(false),
              serverAddedFlag(false),
              stripedFileFlag(stripedFile),
              updateMTimeFlag(updateMTime),
              numServers(nServers),
              numAckMsg(0),
              chunkChecksum(0),
              chunkSize(-1),
              chunkVersion(cvers),
              pendingReqRingTail(0),
              pathname(name)
            {}
        bool         beginMakeStableFlag:1;
        bool         logMakeChunkStableFlag:1;
        bool         serverAddedFlag:1;
        const bool   stripedFileFlag:1;
        const bool   updateMTimeFlag:1;
        int          numServers;
        int          numAckMsg;
        uint32_t     chunkChecksum;
        chunkOff_t   chunkSize;
        seq_t        chunkVersion;
        MetaRequest* pendingReqRingTail;
        const string pathname;
    };
    typedef KVPair<chunkId_t, MakeChunkStableInfo> NonStableChunkKVEntry;
    typedef LinearHash<
        NonStableChunkKVEntry,
        KeyCompare<NonStableChunkKVEntry::Key>,
        DynamicArray<
            SingleLinkedList<NonStableChunkKVEntry>*,
            9 // 2^9
        >,
        StdFastAllocator<NonStableChunkKVEntry>
    > NonStableChunks;

    /// Set of chunks that are in the process being made stable: a
    /// message has been sent to the associated chunkservers which are
    /// flushing out data to disk.
    // Pending make stable -- chunks with no replicas at the moment.
    // Persistent across restarts -- serialized onto transaction log and
    // checkpoint. See make stable protocol description in LayoutManager.cc
    struct PendingMakeStableEntry
    {
        PendingMakeStableEntry(
            chunkOff_t size        = -1,
            bool       hasChecksum = false,
            uint32_t   checksum    = 0,
            seq_t      version     = -1)
            : mSize(size),
              mHasChecksum(hasChecksum),
              mChecksum(checksum),
              mChunkVersion(version)
            {}
        chunkOff_t mSize;
        bool       mHasChecksum;
        uint32_t   mChecksum;
        seq_t      mChunkVersion;
        bool operator==(const PendingMakeStableEntry& rhs) const
        {
            return (
                mSize == rhs.mSize &&
                mHasChecksum == rhs.mHasChecksum &&
                mChecksum == rhs.mChecksum &&
                mChunkVersion == rhs.mChunkVersion
            );
        }
        bool operator!=(const PendingMakeStableEntry& rhs) const
            { return (! (rhs == *this)); }
    };
    typedef KVPair<chunkId_t, PendingMakeStableEntry> PendingMakeStableKVEntry;
    typedef LinearHash<
        PendingMakeStableKVEntry,
        KeyCompare<PendingMakeStableKVEntry::Key>,
        DynamicArray<
            SingleLinkedList<PendingMakeStableKVEntry>*,
            9 // 2^9
        >,
        StdFastAllocator<PendingMakeStableKVEntry>
    > PendingMakeStable;

    typedef KVPair<chunkId_t, seq_t> PendingBeginMakeStableKVEntry;
    typedef LinearHash<
        PendingBeginMakeStableKVEntry,
        KeyCompare<PendingBeginMakeStableKVEntry::Key>,
        DynamicArray<
            SingleLinkedList<PendingBeginMakeStableKVEntry>*,
            9 // 2^9
        >,
        StdFastAllocator<PendingBeginMakeStableKVEntry>
    > PendingBeginMakeStable;

    NonStableChunks        mNonStableChunks;
    PendingBeginMakeStable mPendingBeginMakeStable;
    PendingMakeStable      mPendingMakeStable;

    /// In memory representation of chunk versions roll back.
    typedef KVPair<chunkId_t, seq_t> ChunkVersionRollBackEntry;
    typedef LinearHash<
        ChunkVersionRollBackEntry,
        KeyCompare<ChunkVersionRollBackEntry::Key>,
        DynamicArray<
            SingleLinkedList<ChunkVersionRollBackEntry>*,
            9 // 2^9
        >,
        StdFastAllocator<ChunkVersionRollBackEntry>
    > ChunkVersionRollBack;
    ChunkVersionRollBack mChunkVersionRollBack;

    typedef KVPair<fid_t, MetaSetATime*> SetATimeInFlightEntry;
    typedef LinearHash<
        SetATimeInFlightEntry,
        KeyCompare<SetATimeInFlightEntry::Key>,
        DynamicArray<
            SingleLinkedList<SetATimeInFlightEntry>*,
            8 // 2^8
        >,
        StdFastAllocator<SetATimeInFlightEntry>
    > SetATimeInFlight;
    SetATimeInFlight mSetATimeInFlight;

    /// Counters to track chunk replications
    Counter *mOngoingReplicationStats;
    Counter *mTotalReplicationStats;
    /// how much todo before we are all done (estimate of the size
    /// of the chunk-replication candidates set).
    Counter *mReplicationTodoStats;
    /// # of chunks for which there is only a single copy
    /// Track the # of replication ops that failed
    Counter *mFailedReplicationStats;
    /// Track the # of stale chunks we have seen so far
    Counter *mStaleChunkCount;
    size_t mMastersCount;
    size_t mSlavesCount;
    bool   mAssignMasterByIpFlag;
    int    mLeaseOwnerDownExpireDelay;
    int    mMaxDumpsterCleanupInFlight;
    int    mMaxTruncateChunksDeleteCount;
    int    mMaxTruncateChunksQueueCount;
    int    mMaxTruncatedChunkDeletesInFlight;
    int    mTruncatedChunkDeletesInFlight;
    bool   mWasServicingFlag;
    // Write append space reservation accounting.
    int    mMaxReservationSize;
    int    mReservationDecayStep;
    int    mChunkReservationThreshold;
    int    mAllocAppendReuseInFlightTimeoutSec;
    int    mMinAppendersPerChunk;
    int    mMaxAppendersPerChunk;
    double mReservationOvercommitFactor;
    // Delay replication when connection breaks.
    int    mServerDownReplicationDelay;
    uint64_t mMaxDownServersHistorySize;
    // Chunk server properties broadcasted to all chunk servers.
    Properties mChunkServersProps;
    string     mChunkServersPropsFileName;
    bool       mReloadChunkServersPropertiesFlag;
    // Chunk server restart logic.
    int     mCSToRestartCount;
    int     mMastersToRestartCount;
    int     mMaxCSRestarting;
    bool    mRetireOnCSRestartFlag;
    int64_t mMaxCSUptime;
    int64_t mCSRestartTime;
    int64_t mCSGracefulRestartTimeout;
    int64_t mCSGracefulRestartAppendWithWidTimeout;
    int64_t mLastReplicationCheckTime;
    // "instant du"
    time_t  mLastRecomputeDirsizeTime;
    int     mRecomputeDirSizesIntervalSec;
    /// Max # of concurrent read/write replications per node
    ///  -- write: is the # of chunks that the node can pull in from outside
    ///  -- read: is the # of chunks that the node is allowed to send out
    ///
    int     mMaxConcurrentWriteReplicationsPerNode;
    int     mMaxConcurrentReadReplicationsPerNode;
    bool    mUseEvacuationRecoveryFlag;
    int64_t mReplicationFindWorkTimeouts;
    /// How much do we spend on each internal RPC in chunk-replication-check to handout
    /// replication work.
    int64_t mMaxTimeForChunkReplicationCheck;
    int64_t mMinChunkReplicationCheckInterval;
    int64_t mLastReplicationCheckRunEndTime;
    int64_t mReplicationCheckTimeouts;
    int64_t mNoServersAvailableForReplicationCount;
    /// Periodically (once a week), check the replication of all blocks in the system
    int64_t mFullReplicationCheckInterval;
    bool    mCheckAllChunksInProgressFlag;

    ///
    /// When placing chunks, we see the space available on the node as well as
    /// we take our estimate of the # of writes on
    /// the node as a hint for choosing servers; if a server is "loaded" we should
    /// avoid sending traffic to it.  This value defines a watermark after which load
    /// begins to be an issue.
    ///
    uint32_t      mConcurrentWritesPerNodeWatermark;

    double        mMaxSpaceUtilizationThreshold;
    bool          mUseFsTotalSpaceFlag;
    int64_t       mChunkAllocMinAvailSpace;

    int64_t       mCompleteReplicationCheckInterval;
    int64_t       mCompleteReplicationCheckTime;
    int64_t       mPastEofRecoveryDelay;
    size_t        mMaxServerCleanupScan;
    int           mMaxRebalanceScan;
    double        mRebalanceReplicationsThreshold;
    int64_t       mRebalanceReplicationsThresholdCount;
    int64_t       mMaxRebalanceRunTime;
    int64_t       mLastRebalanceRunTime;
    int64_t       mRebalanceRunInterval;
    size_t        mMaxRebalancePlanRead;
    string        mRebalancePlanFileName;
    RebalanceCtrs mRebalanceCtrs;
    ifstream      mRebalancePlan;
    bool          mCleanupScheduledFlag;
    bool          mPrimaryFlag;

    int                mCSCountersUpdateInterval;
    time_t             mCSCountersUpdateTime;
    IOBuffer           mCSCountersResponse;
    int                mCSDirCountersUpdateInterval;
    time_t             mCSDirCountersUpdateTime;
    IOBuffer           mCSDirCountersResponse;
    int                mPingUpdateInterval;
    time_t             mPingUpdateTime;
    IOBuffer           mPingResponse;
    IOBuffer::WOStream mWOstream;
    QCIoBufferPool*    mBufferPool;
    bool               mMightHaveRetiringServersFlag;

    typedef HostPrefixMap<RackId>      RackPrefixes;
    typedef map<int, double>           RackWeights;
    typedef vector<string>             ChunkServersMd5sums;
    bool                mRackPrefixUsePortFlag;
    bool                mUseCSRackAssignmentFlag;
    bool                mReplaySetRackFlag;
    RackPrefixes        mRackPrefixes;
    RackWeights         mRackWeights;
    ChunkServersMd5sums mChunkServerMd5sums;
    string              mClusterKey;

    int64_t mDelayedRecoveryUpdateMaxScanCount;
    bool    mForceDelayedRecoveryUpdateFlag;
    bool    mSortCandidatesBySpaceUtilizationFlag;
    bool    mSortCandidatesByLoadAvgFlag;
    int64_t mMaxFsckFiles;
    int64_t mFsckAbandonedFileTimeout;
    int64_t mMaxFsckTime;
    bool    mFullFsckFlag;
    int64_t mDirATimeUpdateResolution;
    int64_t mATimeUpdateResolution;
    int64_t mMTimeUpdateResolution;
    int64_t mMaxPendingRecoveryMsgLogInfo;
    bool    mAllowLocalPlacementFlag;
    bool    mAllowLocalPlacementForAppendFlag;
    bool    mInRackPlacementForAppendFlag;
    bool    mInRackPlacementFlag;
    bool    mAppendPlacementIgnoreMasterSlaveFlag;
    bool    mAllocateDebugVerifyFlag;

    CSMap::Entry* mChunkEntryToChange;
    MetaFattr*    mFattrToChangeTo;

    int64_t mCSLoadAvgSum;
    int64_t mCSMasterLoadAvgSum;
    int64_t mCSSlaveLoadAvgSum;
    int64_t mCSTotalLoadAvgSum;
    int64_t mCSOpenObjectCount;
    int64_t mCSWritableObjectCount;
    int     mCSTotalPossibleCandidateCount;
    int     mCSMasterPossibleCandidateCount;
    int     mCSSlavePossibleCandidateCount;
    bool    mUpdateCSLoadAvgFlag;
    bool    mUpdatePlacementScaleFlag;
    int64_t mCSMaxGoodCandidateLoadAvg;
    int64_t mCSMaxGoodMasterCandidateLoadAvg;
    int64_t mCSMaxGoodSlaveCandidateLoadAvg;
    double  mCSMaxGoodCandidateLoadRatio;
    double  mCSMaxGoodMasterLoadRatio;
    double  mCSMaxGoodSlaveLoadRatio;
    int64_t mSlavePlacementScale;
    int64_t mMaxSlavePlacementRange;
    int16_t mMaxReplicasPerFile;
    int16_t mMaxReplicasPerRSFile;
    bool    mGetAllocOrderServersByLoadFlag;
    int     mMinChunkAllocClientProtoVersion;

    int     mMaxResponseSize;
    int64_t mMinIoBufferBytesToProcessRequest;
    int     mReadDirLimit;
    bool    mAllowChunkServerRetireFlag;
    bool    mPanicOnInvalidChunkFlag;
    bool    mPanicOnRemoveFromPlacementFlag;
    int     mAppendCacheCleanupInterval;
    int     mTotalChunkWrites;
    int     mTotalWritableDrives;
    int     mMinWritesPerDrive;
    int     mMaxWritesPerDrive;
    int     mMaxWritesPerDriveThreshold;
    double  mMaxWritesPerDriveRatio;
    double  mMaxLocalPlacementWeight;
    double  mTotalWritableDrivesMult;
    string  mConfig;
    Properties mConfigParameters;

    kfsUid_t  mDefaultUser;
    kfsGid_t  mDefaultGroup;
    kfsMode_t mDefaultFileMode;
    kfsMode_t mDefaultDirMode;
    kfsUid_t  mDefaultLoadUser;
    kfsGid_t  mDefaultLoadGroup;
    kfsMode_t mDefaultLoadFileMode;
    kfsMode_t mDefaultLoadDirMode;
    bool      mForceEUserToRootFlag; // Turns off permission verification.
    bool      mVerifyAllOpsPermissionsParamFlag;
    bool      mVerifyAllOpsPermissionsFlag; // If true, then the

    // integrate for oec
    string useOEC;
    unsigned int localIp;
    unsigned int coorIp;

    // following won't work:
    // write(open("/file", O_RDWR | O_CREAT, 0000), "1", 1);
    typedef set<string> RootHosts;
    RootHosts mRootHosts;
    struct HostUserGroupMapEntry
    {
        HostUserGroupMapEntry()
            : mHostPrefix(),
              mUserMap(),
              mGroupMap()
            {}
        typedef map<kfsUid_t, kfsUid_t> UserMap;
        typedef map<kfsUid_t, kfsUid_t> GroupMap;
        HostPrefix mHostPrefix;
        UserMap    mUserMap;
        GroupMap   mGroupMap;
    };
    typedef vector<HostUserGroupMapEntry> HostUserGroupRemap;
    HostUserGroupRemap mHostUserGroupRemap;
    struct LastUidGidRemap
    {
        string   mIp;
        kfsUid_t mUser;
        kfsGid_t mGroup;
        kfsUid_t mToUser;
        kfsGid_t mToGroup;
        LastUidGidRemap()
            : mIp(),
              mUser(kKfsUserNone),
              mGroup(kKfsGroupNone),
              mToUser(kKfsUserNone),
              mToGroup(kKfsGroupNone)
             {}
    };
    LastUidGidRemap mLastUidGidRemap;

    volatile int64_t  mIoBufPending;
    volatile uint64_t mAuthCtxUpdateCount;
    AuthContext       mClientAuthContext;
    AuthContext       mCSAuthContext;
    UserAndGroup      mUserAndGroup;
    bool              mClientCSAuthRequiredFlag;
    bool              mClientCSAllowClearTextFlag;
    int               mCSAccessValidForTimeSec;
    int               mMinWriteLeaseTimeSec;
    bool              mFileSystemIdRequiredFlag;
    bool              mDeleteChunkOnFsIdMismatchFlag;
    int               mChunkAvailableUseReplicationOrRecoveryThreshold;
    bool              mObjectStoreEnabledFlag;
    bool              mObjectStoreReadCanUsePoxoyOnDifferentHostFlag;
    bool              mObjectStoreWriteCanUsePoxoyOnDifferentHostFlag;
    bool              mObjectStorePlacementTestFlag;

    typedef set<int> CreateFileTypeExclude;
    CreateFileTypeExclude mCreateFileTypeExclude;
    int                   mMaxDataStripeCount;
    int                   mMaxRecoveryStripeCount;
    int                   mMaxRSDataStripeCount;
    int64_t               mDebugSimulateDenyHelloResumeInterval;
    int64_t               mDebugPanicOnHelloResumeFailureCount;
    string                mHelloResumeFailureTraceFileName;

    typedef MetaChunkReplicate::FileRecoveryInFlightCount
        FileRecoveryInFlightCount;
    FileRecoveryInFlightCount mFileRecoveryInFlightCount;
    IdempotentRequestTracker mIdempotentRequestTracker;

    RequestQueue mResubmitQueue;

    int                      mObjStoreDeleteMaxSchedulePerRun;
    int                      mObjStoreMaxDeletesPerServer;
    int                      mObjStoreDeleteDelay;
    bool                     mResubmitClearObjectStoreDeleteFlag;
    size_t                   mObjStoreDeleteSrvIdx;
    ObjStoreFilesDeleteQueue mObjStoreFilesDeleteQueue;
    ObjBlocksDeleteRequeue   mObjBlocksDeleteRequeue;
    ObjBlocksDeleteInFlight  mObjBlocksDeleteInFlight;
    ChunkServerPtr           mRestoreChunkServerPtr;
    HibernatedChunkServerPtr mRestoreHibernatedCSPtr;
    size_t                   mReplayServerCount;
    size_t                   mDisconnectedCount;
    time_t                   mServiceStartTime;
    bool                     mCleanupFlag;

    StTmp<vector<MetaChunkInfo*> >::Tmp mChunkInfosTmp;
    StTmp<vector<MetaChunkInfo*> >::Tmp mChunkInfos2Tmp;
    StTmp<Servers>::Tmp                 mServersTmp;
    StTmp<Servers>::Tmp                 mServers2Tmp;
    StTmp<Servers>::Tmp                 mServers3Tmp;
    StTmp<Servers>::Tmp                 mServers4Tmp;
    StTmp<vector<kfsSTier_t> >::Tmp     mPlacementTiersTmp;

    class ChunkPlacement : public KFS::ChunkPlacement<LayoutManager>
    {
    public:
        typedef KFS::ChunkPlacement<LayoutManager> Super;
        ChunkPlacement(LayoutManager* layoutManager = 0);
    };
    StTmp<ChunkPlacement>::Tmp mChunkPlacementTmp;

    PrngIsaac64     mRandom;
    ostringstream   mTempOstream;
    StorageTierInfo mStorageTierInfo[kKfsSTierCount];
    double          mTierSpaceUtilizationThreshold[kKfsSTierCount];
    int             mTiersMaxWritesPerDriveThreshold[kKfsSTierCount];
    int             mTiersMaxWritesPerDrive[kKfsSTierCount];
    double          mTiersTotalWritableDrivesMult[kKfsSTierCount];
    int             mTierCandidatesCount[kKfsSTierCount];

    ostringstream& GetTempOstream()
    {
        mTempOstream.str(string());
        mTempOstream.clear();
        mTempOstream.flags(ostream::dec | ostream::skipws);
        mTempOstream.precision(6);
        mTempOstream.width(0);
        mTempOstream.fill(' ');
        mTempOstream.tie(0);
        return mTempOstream;
    }

    void DeleteNonStableEntry(
        chunkId_t                  chunkId,
        const MakeChunkStableInfo* it,
        int                        status    = 0,
        const char*                statusMsg = 0);
    /// Check the # of copies for the chunk and return true if the
    /// # of copies is less than targeted amount.  We also don't replicate a chunk
    /// if it is currently being written to (i.e., if a write lease
    /// has been issued).
    /// @param[in] clli  The location information about the chunk.
    /// @param[out] extraReplicas  The target # of additional replicas for the chunk
    /// @retval true if the chunk is to be replicated; false otherwise
    bool CanReplicateChunkNow(
        CSMap::Entry&      clli,
        int&               extraReplicas,
        ChunkPlacement&    chunkPlacement,
        int*               hibernatedReplicaCount = 0,
        ChunkRecoveryInfo* recoveryInfo           = 0,
        bool               forceRecoveryFlag      = false);

    /// Replicate a chunk.  This involves finding a new location for
    /// the chunk that is different from the existing set of replicas
    /// and asking the chunkserver to get a copy.
    /// @param[in] chunkId   The id of the chunk which we are checking
    /// @param[in] clli  The lease/location information about the chunk.
    /// @param[in] extraReplicas  The target # of additional replicas for the chunk
    /// @param[in] candidates   The set of servers on which the additional replicas
    ///                 should be stored
    /// @retval  The # of actual replications triggered
    int ReplicateChunk(
        CSMap::Entry&            clli,
        int                      extraReplicas,
        ChunkPlacement&          chunkPlacement,
        const ChunkRecoveryInfo& recoveryInfo);
    int ReplicateChunk(
        CSMap::Entry&             clli,
        int                       extraReplicas,
        const Servers&            candidates,
        const ChunkRecoveryInfo&  recoveryInfo,
        const vector<kfsSTier_t>& tiers,
        kfsSTier_t                maxSTier,
        const char*               reasonMsg = 0,
        bool                      removeReplicaFlag = false);

    /// From the candidates, handout work to nodes.  If any chunks are
    /// over-replicated/chunk is deleted from system, add them to delset.
    bool HandoutChunkReplicationWork();

    /// There are more replicas of a chunk than the requested amount.  So,
    /// delete the extra replicas and reclaim space.  When deleting the addtional
    /// copies, find the servers that are low on space and delete from there.
    /// As part of deletion, we update our mapping of where the chunk is stored.
    /// @param[in] chunkId   The id of the chunk which we are checking
    /// @param[in] clli  The lease/location information about the chunk.
    /// @param[in] extraReplicas  The # of replicas that need to be deleted
    void DeleteAddlChunkReplicas(CSMap::Entry& entry, int extraReplicas,
        ChunkPlacement& placement);

    /// Helper function to check set membership.
    /// @param[in] hosters  Set of servers hosting a chunk
    /// @param[in] server   The server we want to check for membership in hosters.
    /// @retval true if server is a member of the set of hosters;
    ///         false otherwise
    bool IsChunkHostedOnServer(const Servers &hosters,
                    const ChunkServerPtr &server);

    /// Periodically, update our estimate of how much space is
    /// used/available in each rack.
    void UpdateRackSpaceUsageCounts();

    /// Does any server have space/write-b/w available for
    /// re-replication
    int CountServersAvailForReReplication(
        size_t& pendingHelloCnt, size_t& connectedCnt) const;

    /// Periodically, rebalance servers by moving chunks around from
    /// "over utilized" servers to "under utilized" servers.
    void RebalanceServers();
    void UpdateReplicationsThreshold();

    /// For a time period that corresponds to the length of a lease interval,
    /// we are in recovery after a restart.
    /// Also, if the # of chunkservers that are connected to us is
    /// less than some threshold, we are in recovery mode.
    inline bool InRecovery() const;
    inline bool InRecoveryPeriod() const;

    inline bool IsChunkServerRestartAllowed() const;
    void ScheduleChunkServersRestart();
    bool AddReplica(CSMap::Entry& entry, const ChunkServerPtr& c);
    void CheckChunkReplication(CSMap::Entry& entry);
    inline void UpdateReplicationState(CSMap::Entry& entry);
    inline void SetReplicationState(CSMap::Entry& entry, CSMap::Entry::State state);

    inline seq_t GetChunkVersionRollBack(chunkId_t chunkId);
    inline seq_t IncrementChunkVersionRollBack(chunkId_t chunkId);
    inline void UpdatePendingRecovery(CSMap::Entry& entry);
    inline void CheckReplication(CSMap::Entry& entry);
    bool GetPlacementExcludes(const CSMap::Entry& entry, ChunkPlacement& placement,
        bool includeThisChunkFlag = true,
        bool stopIfHasAnyReplicationsInFlight = false,
        vector<MetaChunkInfo*>* chunkBlock = 0);
    void ProcessInvalidStripes(MetaChunkReplicate& req);
    RackId GetRackId(const ServerLocation& loc) const;
    RackId GetRackId(const string& loc) const;
    RackId GetRackId(const MetaRequest& req) const;
    void ScheduleCleanup(size_t maxScanCount = 1);
    void RemoveRetiring(CSMap::Entry& ci, Servers& servers, int numReplicas,
        bool deleteRetiringFlag = false);
    void DeleteChunk(fid_t fid, chunkId_t chunkId, const Servers& servers,
        bool staleChunkIdFlag = false);
    void UpdateGoodCandidateLoadAvg();
    inline static CSMap::Entry& GetCsEntry(MetaChunkInfo& chunkInfo);
    inline static CSMap::Entry* GetCsEntry(MetaChunkInfo* chunkInfo);
    bool CanBeRecovered(
        const CSMap::Entry&     entry,
        bool&                   incompleteChunkBlockFlag,
        bool*                   incompleteChunkBlockWriteHasLeaseFlag,
        vector<MetaChunkInfo*>& cblk,
        int*                    outGoodCnt = 0) const;
    HibernatingServerInfo* FindHibernatingCSInfo(const ServerLocation& loc,
        HibernatedServerInfos::iterator* outIt = 0);
    HibernatedChunkServer* FindHibernatingCS(const ServerLocation& loc,
        HibernatedServerInfos::iterator* outIt = 0);
    void CSMapUnitTest(const Properties& props);
    int64_t GetMaxCSUptime() const;
    bool ReadRebalancePlan(size_t nread);
    bool Fsck(ostream &os, bool reportAbandonedFilesFlag);
    void CheckFile(
        FilesChecker&     fsck,
        const MetaDentry& de,
        const MetaFattr&  fa);
    template<typename T, typename OT> void LoadIdRemap(
        istream& fs, T OT::* map);
    void SetUserAndGroupSelf(const MetaRequest& req,
        kfsUid_t& user, kfsGid_t& group);
    RackInfos::iterator FindRack(RackId id) {
        return FindRackT(mRacks.begin(), mRacks.end(), id);
    }
    RackInfos::const_iterator FindRack(RackId id) const {
        return FindRackT(mRacks.begin(), mRacks.end(), id);
    }
    bool FindStorageTiersRange(kfsSTier_t& minTier, kfsSTier_t& maxTier);
    bool UpdateClientAuth(AuthContext& ctx);
    void MakeChunkAccess(
        const CSMap::Entry&            cs,
        kfsUid_t                       authUid,
        MetaLeaseAcquire::ChunkAccess& chunkAccess,
        const ChunkServer*             writeMaster);
    void MakeChunkAccess(
        chunkId_t                      chunkId,
        const LayoutManager::Servers&  servers,
        kfsUid_t                       authUid,
        MetaLeaseAcquire::ChunkAccess& chunkAccess,
        const ChunkServer*             writeMaster);
    bool IsVerificationOfStatsOrAdminPermissionsRequired(MetaRequest& op)
    {
        if (! mVerifyAllOpsPermissionsFlag) {
            return false;
        }
        if ((mClientAuthContext.IsAuthRequired() &&
                    op.authUid == kKfsUserNone)) {
            // Authentication was forfeited by client sm code.
            return false;
        }
        SetEUserAndEGroup(op);
        return true;
    }
    void ScheduleResubmitOrCancel(MetaRequest& r);
    inline bool AddHosted(CSMap::Entry& entry, const ChunkServerPtr& c,
        size_t* srvCount = 0);
    inline bool AddHosted(chunkId_t chunkId, CSMap::Entry& entry,
        const ChunkServerPtr& c);
    bool AddServerWithStableReplica(
        CSMap::Entry& c, const ChunkServerPtr& server);
    bool RunObjectBlockDeleteQueue();
    chunkOff_t DeleteFileBlocks(fid_t fid, chunkOff_t first, chunkOff_t last,
        int& remScanCnt);
    inline Servers::const_iterator FindServer(const ServerLocation& loc) const;
    template<typename T>
    inline Servers::const_iterator FindServerByHost(const T& host) const;
    bool FindAccessProxy(MetaAllocate& req);
    bool FindAccessProxyFor(const MetaRequest& req, Servers& srvs);
    template <typename T>
    bool GetAccessProxyFromReq(T& req, Servers& servers);
    void Replay(MetaHello& req);
    void SetRack(const ChunkServerPtr& server, RackId rackId);
    template<typename T> const ChunkServerPtr* ReplayFindServer(
        const ServerLocation& loc, T& req);
    template<typename T> bool HandleReplay(T& req);
    size_t GetConnectedServerCount() const
    {
        return (mChunkServers.size() - mReplayServerCount -
            mDisconnectedCount);
    }
    inline bool RemoveServer(
        const ChunkServerPtr& server, bool replayFlag, chunkId_t chunkId);
    inline bool IsAllocationInFlight(chunkId_t chunkId);
    void ScheduleTruncatedChunksDelete();
    void CleanupChunkServers();
    bool AddToInFlightChunkAllocation(
        const MetaAllocate& req, const ChunkServerPtr& server);
    inline LayoutManager::Servers::const_iterator FindServerForReq(
        const MetaRequest& req);
    template<typename T> inline void UpdateATimeSelf(
        int64_t updateResolutionUsec, const MetaFattr* fa, T& req);
private:
    LayoutManager(const LayoutManager&);
    LayoutManager& operator=(LayoutManager);
};

extern LayoutManager& gLayoutManager;
}

#endif // META_LAYOUTMANAGER_H
