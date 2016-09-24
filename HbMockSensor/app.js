var mqtt = require("mqtt");
var client = mqtt.connect();

// Extract parameters from the start arguments


client.on("connect", function () {
    client.subscribe("testTopic");
    client.publish("testTopic", "Connected");
});

client.on("message", function (topic, message) {
    console.log(topic, message.toString());
    client.end();
    process.exit();
})