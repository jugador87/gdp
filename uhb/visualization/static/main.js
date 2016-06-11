// All javascript functions go here...
google.charts.load('current', {'packages':['corechart', 'controls']});

deviceData = {
        "logs": [
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300003", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5700088", "type": "PowerBlade", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570008b", "type": "PowerBlade", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570008e", "type": "PowerBlade", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e590000a", "type": "Blink", "humanName": 'xyz'},


            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530000a", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e590000b", "type": "Blink", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5900019", "type": "Blink", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5900091", "type": "Blink", "humanName": 'xyz'},


            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300009", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300036", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300054", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e530005d", "type": "BLEES", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e570008f", "type": "PowerBlade", "humanName": 'xyz'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e590001e", "type": "Blink", "humanName": 'xyz'}],
    };


function makeForm() {
    // Code for picking up a device
    var devicePickerString = "";
    for (var i=0; i<deviceData.logs.length; i++) {
        var log = deviceData.logs[i];
        devicePickerString += "<option value='" + log.logname + "'>" + log.logname + "</option>";
    }
    document.getElementById('logname').innerHTML = devicePickerString;
    var timeNow = Date.now()
    document.getElementById('startTime').value = new Date(timeNow-3600000).toLocaleString();
    document.getElementById('endTime').value = new Date(timeNow).toLocaleString();
}

function plot(form) {
    var logname = form.logname.value;
    startTime = Date.parse(form.startTime.value)/1000.0;
    endTime = Date.parse(form.endTime.value)/1000.0;

    if (typeof dashboard != 'undefined') {
        for (var i=0; i<pChartArray.length; i++) {
            pChartArray[i].visualization.clearChart();
    	}
        pSlider.visualization.dispose();
        dashboard.dispose();
    }

    drawChart(logname, startTime, endTime);
}

function drawChart(logname, startTime, endTime) {

    var baseurl = "/datasource?";

    var query_string = "";
    query_string += "logname=" + String(logname) + "&"
    query_string += "startTime=" + String(startTime) + "&"
    query_string += "endTime=" + String(endTime)

    var query = new google.visualization.Query(baseurl+query_string);
    query.send(handleQueryResponse);
    document.getElementById('request_status').innerHTML = "Request sent...";
}

function handleQueryResponse(response) {

    document.getElementById('request_status').innerHTML = "The first graph is an overview graph with controllable sliders to zoom in/out, followed by individual parameters plotted separately.";

    data = response.getDataTable();

    dashboard = new google.visualization.Dashboard(
        document.getElementById('dashboard_div'));

    pSlider =  new google.visualization.ControlWrapper({
        'controlType': 'ChartRangeFilter',
        'containerId': 'control_div',
        'options': {
            'filterColumnLabel': 'time',
            //'ui': {'labelStacking': 'vertical'}
        },
        'state': {
            'range': {'start': new Date(((startTime+endTime)/2)*1000),
                      'end': new Date(endTime*1000)},
        }
    });

    pChartArray = [];
    var colors = ['black', 'blue', 'red', 'green', 'yellow', 'gray'];
    for (var i=1; i<data.getNumberOfColumns(); i++) {
        var pChart = new google.visualization.ChartWrapper({
        'chartType': 'LineChart',
        'containerId': 'chart_div_'+String(i),
        'view': {'columns': [0, i]},
        'options': {
            'colors': [colors[(i-1)%colors.length]],
            'title' : data.getColumnLabel(i),
            'pointSize': 3,
            //'curveType' : 'function',
            //'legend': { 'position': 'bottom'}
            }
        });

        pChartArray.push(pChart);
    }
    dashboard.bind(pSlider, pChartArray);
    dashboard.draw(data);
}

