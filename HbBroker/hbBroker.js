var mqtt = require('mqtt');
var fs = require('fs');
var PouchDB = require('pouchdb');

// Load the settings synchronously since we can't do anything until we've got them
var settings = {
    topicPrefix: '/hb/',
    dbPrefix: 'mqtt/hb/',
    mqttBrokerUrl: 'mqtt://localhost/',
    dbUrl: 'http://admin:brilt@localhost:5984/',
    dbName: 'HameBoss'
}
if (fs.existsSync('./hbBroker.json')) {
    let options = JSON.parse(fs.readFileSync('./hbBroker.json'));
    for (let key in options) {
        if (options.hasOwnProperty(key)) {
            settings[key] = options[key];
        }
    }
}

var db = new PouchDB(settings.dbUrl + settings.dbName.toLowerCase(), {});
var client = mqtt.connect(settings.mqttBrokerUrl);

client.on('connect', function () {
    console.log('Connected to MQTT broker, ', settings.mqttBrokerUrl);
    client.subscribe(settings.topicPrefix + '#');
    console.log('Subscribed to MQTT topics, ', settings.topicPrefix + '#');
});

db.info().then(function (info) {
    console.log('Connected to database, ', info.db_name);

    // Handle MQTT messages
    client.on('message', function (topic, message) {
        // Remove the prefix from the topic
        topic = topic.substr(settings.topicPrefix.length);
        let msg = message.toString();
        console.log(topic, msg);

        if (topic === 'sys/run' && msg === 'stop') {
            process.exit();
        }

        let doc;
        let doc_id = settings.dbPrefix + topic;
        db.get(doc_id).then(function (data) {
            doc = data;
            if (doc.rw === 'r') {
                doc.msg = msg;
                return db.put(doc);
            }
        }).catch(function (err) {
            if (err.status === 404) {
                doc = {
                    _id: doc_id,
                    msg: msg,
                    rw: 'r'
                };
                return db.put(doc);
            } else {
                console.log(err);
            }
        }).then(function () {
            if (doc.rw === 'r') {
                console.log(msg, ' saved to ', doc_id);
            }
        });
    });

    // Handle db changes
    db.changes({
        since: 'now',
        live: true,
        include_docs: true
    }).on('change', function (change) {
        if (!change.deleted && RegExp(settings.dbPrefix + '.*', 'i').test(change.doc._id) && change.doc.rw === 'w') {
            let topic = settings.topicPrefix + change.doc._id.substr(settings.dbPrefix.length);
            client.publish(topic, change.doc.msg, function (err) {
                if (err) {
                    console.log(err);
                } else {
                    console.log(change.doc.msg, ' published to ', topic);
                }
            });
        }
    }).on('error', function (err) {
        console.log(err);
    })
})
