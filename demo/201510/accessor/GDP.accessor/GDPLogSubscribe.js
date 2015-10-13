/* Accessor for a log */

var GDP = require('GDP');
var log = null;
var handle = null;

exports.setup = function() {
    input('trigger');
    output('data', {'type': 'string'});
    parameter('logname', {'type': 'string'});
    parameter('startrec', {'type': 'int', 'value': 0});
    parameter('numrec', {'type': 'int', 'value':0});
};

exports.get_next_data = function() {
    // this blocks
    var data = log.get_next_data(0);
    send('data', data); 
}


exports.initialize = function() {

    var logname = getParameter('logname');
    log = GDP.GDP(logname, 1);
    log.subscribe(getParameter('startrec'), getParameter('numrec'));
    handle = addInputHandler('trigger', this.get_next_data);
}

exports.wrapup = function() {
    if (handle != null) {
        removeInputHandler(handle);
    }
}
