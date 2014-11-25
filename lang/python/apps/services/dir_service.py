#!/usr/bin/python

"""
Provides a user with a directory service for his/her logs in GDP. The 
    data is backed by another GCL in the GDP, and thus all that a user
    needs to do is: launch one instance of this service.

This is a low performance, single threaded service. 

This service supports the following commands for the user:
- ls            : list all the available human friendly GCL names
- newgcl <name> : create a new GCL with the given human friendly name, 
                    returns a 256 bit name
- getgcl <name> : get the corresponding 256 bit name for the given
                    human friendly name

"""

import sys
sys.path.append("../../")

import wrapper as gdp
import SocketServer

class RequestHandler(SocketServer.BaseRequestHandler):
    """ Listen to the request and returns responses """


    def update_mapping(self, (human_name, printable_name)):
        """
        Add a new mapping of a human name to an internal name. If the 
            human name already exists, update the old mapping to the
            new mapping. 
        """

        # TODO: make these two operations somewhat atomic.

        # write to GCL
        gcl_datum = {'data': human_name+"=>"+printable_name}
        self.server.gcl_handle.publish(gcl_datum)

        # update the dictionary
        self.server.mapping[human_name] = printable_name

        return


    def handle_command(self, cmd):

        temp = cmd.split()
        return_message = ""

        if temp[0] == "ls":

            arr = []
            for k in self.server.mapping.keys():
                arr.append(k+"=>"+self.server.mapping[k])

            return_message = "\n".join(arr)

        elif temp[0] == "getgcl":

            if len(temp)<2:
                return_message = "Bad Arguments"
            else:
                return_message = self.server.mapping.get(temp[1],"Doesn't exist")

        elif temp[0] == "newgcl":

            if len(temp)<2:
                return_message = "Bad Arguments"
            else:
                new_gcl_handle = gdp.GDP_GCL(create=True, name=None)
                new_name = new_gcl_handle.getname().printable_name()
                self.update_mapping((temp[1], new_name))
                return_message = new_name

        return return_message + "\n\n"
        

    def handle(self):
        """ For now, converts input to upper case and sends it back """

        self.data = self.request.recv(1024).strip()

        # XXX: use the self.server.gcl_handle to read/write data

        print "{} wrote:".format(self.client_address[0])
        print self.data
        cmd_response = self.handle_command(self.data)
        self.request.sendall(cmd_response)




def main(name_str, host, port):

    # name_str of our root directory is passed as a human readable string
    gcl_name = gdp.GCL_NAME(name_str)

    # let's open this gcl
    try:
        gcl_handle = gdp.GDP_GCL(create=True, name=gcl_name)
    except:
        gcl_handle = gdp.GDP_GCL(open=True, name=gcl_name, iomode=gdp.GDP_MODE_AO)


    # create a mapping between human name and internal names
    print "Loading the exisiting GCL into memory"
    mapping = {}
    recno = 1

    while True:

        try:
            datum = gcl_handle.read(recno)
        except:
            break
        data = datum["data"]
        temp = data.split("=>")
        mapping[temp[0]] = temp[1]
        recno += 1
 
    print "Finished reading the exisiting GCL" 

    # Create the server
    server = SocketServer.TCPServer((host, port), RequestHandler)

    # pass a reference to gcl_handle and mapping to server
    server.gcl_handle = gcl_handle
    server.mapping = mapping

    # serve requests
    server.serve_forever()




if __name__=="__main__":

    if len(sys.argv)!=4:
        print "Usage: "+sys.argv[0]+" <root-dir-log> <host> <port>"
        sys.exit(1)
    else:
        main(sys.argv[1], sys.argv[2], int(sys.argv[3]))
