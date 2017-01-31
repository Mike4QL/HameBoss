var mqtt = require('mqtt');

var client = mqtt.connect('mqtt://localhost');

client.on('connect', function() {
    console.log('Connected to MQTT broker.');
    client.subscribe('test');
});

client.on('message', function(topic, message){
    console.log(topic, message.toString());
});
