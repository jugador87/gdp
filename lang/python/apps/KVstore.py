#!/usr/bin/env python

"""

A key value store with checkpoints. All persistent data is stored in a log
called the root. For performance, an in-memory copy is maintained. At the
initilization, the name of the rootlog is to be supplied by a user.
If the assumption of a single-writer is violated, things won't work anymore.

Individual records are serialized versions of python data structures.  There
are multiple kinds of records:

### Typical record ###

A typical record that adds/updates/deletes one or more keys in the KVstore.

{   key1: val1,
    key2: val2,
    ...
}

### Checkpoint record ###

A record that provides a snapshot of a certain record range in the KVstore.
Each checkpoint has a level associated with it, where level is an integer from
0-9. The levels behave pretty much the same way as the unix 'dump' utility ---
level 0 means a full copy of the data upto the current record, any level above
0 provides a snapshot of all the data since last dump of a lower level.

Each checkpoint contains some metadata, especially the current checkpoint level
number and the range of the first and last record included in the checkpoint.

The checkpoint record looks as follows:

( <metadata>, <snapshot> )
where <metadata> is a dictionary of the following type:
{
    cp_range: (<cp_from>, <cp_upto>),
    cp_level: <int>,
}

and <snapshot> is a dictionary like any typical record.

## TODO: 
- handle non-existent keys better than None
- expire entries from the cache
- tune the condition when to merge checkpoint records
- create an intermediate cache


"""

import gdp      # load the main package
import cPickle  # for serialization
import collections
import copy
import random

class KVstore:

    __freq__ = 10

    @staticmethod
    def __to_datum(ds):
        """
        serialize a data structure to string and create a 'datum' object 
            that can be passed on to the GDP append.
        """
        datum = {"data": cPickle.dumps(ds)}
        return datum


    @staticmethod
    def __from_datum(datum):
        """
        take the data out of a gdp datum, unserialize the data structure
            and return that data structure
        """
        ds = cPickle.loads(datum["data"])
        return ds



    def __init__(self, root):
        "Initialize the instance with the root log"
    
        gdp.gdp_init()  ## XXX: Not sure if this is the best idea
        self.root = gdp.GDP_NAME(root)
        self.root_handle = gdp.GDP_GCL(self.root, gdp.GDP_MODE_AO)

        # a cache for records. recno => datum
        # datum may or may not contain a timestamp and recno
        self.cache = {}

        # find the number of records by querying the most recent record
        try:
            datum = self.root_handle.read(-1)
            self.num_records = datum["recno"]
            self.cache[self.num_records] = datum
        except gdp.MISC.EP_STAT_SEV_ERROR:
            self.num_records = 0


    def __read(self, recno):
        "Read a single record. A wrapper around the cache"

        if recno<0: recno = self.num_records+recno
        if recno not in self.cache:
            datum = self.root_handle.read(recno)
            self.cache[recno] = datum
        return self.__from_datum(self.cache[recno])


    def __append(self, ds):
        "Append a single record. A wrapper around the cache"

        datum = self.__to_datum(ds)
        self.root_handle.append(datum)

        self.num_records += 1
        self.cache[self.num_records] = datum


    def __setitems__(self, kvpairs):
        """
        Update multiple values at once. At the core of all updates.
            kvpairs: a dictionary of key=>value mappings
        """

        if (self.num_records+1)%self.__freq__==0:
            self.__do_checkpoint()

        # make sure input is in the desired form
        assert isinstance(kvpairs, dict)
        self.__append(kvpairs)

    
    def __setitem__(self, key, value):
        "set key to value"
        return self.__setitems__({key: value})


    def __delitem__(self, key):
        "remove an existing entry in the log"
        return self.__setitem__({key: None})



    def __getitem__(self, key):
        "get value of key"

        # sequentially read rcords from the very end till we find
        #   the key we are looking for, or we hit a checkpoint record

        cur = self.num_records

        while cur>0:

            rec = self.__read(cur)

            if isinstance(rec, dict):       # regular record
                if key in rec:              # found it
                    return rec[key]
                else:                       # go check the previous record
                    cur = cur-1
                    continue

            elif isinstance(rec, tuple):    # checkpoint record
                (metadata, data) = rec
                assert metadata["cp_range"][1] == cur-1     # sanity check

                if key in data:             # found key in the current CP rec
                    return data[key]
                else:                       # skip the range this CP covers
                    cur = metadata["cp_range"][0]-1
                    continue

            else:                           # unknown type
                assert False

        return None                         # key not found anywhere


    def __do_checkpoint(self):
        """Perform a checkpoint at certain level. 0 means full copy, anything
        above 0 includes data since last checkpoint of a lower level."""

        level = 9
        newmetadata = {}
        newdata = {}
        upper = cur = self.num_records

        while cur>0:

            rec = self.__read(cur)

            if isinstance(rec, dict):       # a regular record
                for key in rec:             # recent data takes precedence
                    if key not in newdata: newdata[key] = rec[key]
                cur = cur-1
                continue 

            elif isinstance(rec, tuple):    # checkpoint record
                (metadata, data) = rec
                assert metadata["cp_range"][1] == cur-1     # sanity check
                assert 0<=metadata["cp_level"]<=9

                if metadata["cp_level"]<level:  # this is where we could stop

                    # but let's check if there is any merit in bumping 
                    #   the level down by combining the two checkpoints
                    ok = data.keys()            # ok => old keys
                    nk = newdata.keys()         # nk => new keys

                    # condition when merging should be performed:
                    if len(set(ok) & set(nk))> 0.8*min(len(ok), len(nk)):

                        # YES, let's merge
                        data.update(newdata)    # dictionary update
                        newdata = data
                        level = metadata["cp_level"]

                        # continue recursively
                        cur = metadata["cp_range"][0]-1
                        continue

                    else:
                        level = metadata["cp_level"]+1
                        break

                else:
                    for key in data:
                        if key not in newdata: newdata[key] = data[key]
                    cur = metadata["cp_range"][0]-1     # skip range covered
                    continue

            else:                           # unknown type
                assert False

        lower = cur+1       # out of loop because either cur==0, or break

        if lower==1: level = 0          # Makes sense, doesn't it?
        newmetadata["cp_range"] = (lower, upper)
        newmetadata["cp_level"] = level
        newrec = (newmetadata, newdata)
        self.__append(newrec)

