# HbGateway
Provides a link between MQTT and the HameBoss databse.

## How it Works
HbGateway subscribes to all topics on the MQTT Broker whose name starts with the Topic Root.
The messages received are added to the CouchDB database using an ID comprising the DB Root with the fragment after the Topic Root and the date/time appended.  So if the Topic Root was "/MyHome/Devices/" and the DB Root was "devices/" a message received on the topic "/MyHome/Devices/temp1" would be added to the database with the ID "devices/temp1/<date & time received>".

## Configuration
The default configuration file is called, *HbGateway.config.json*, in the current folder unless another folder has been specified in the command-line arguments.

| Parameter | Description |
|-----------|-------------|
| **topicRoot** | The root name of the MQTT topic to be monitored.  Messages in any topic beginning with this will be processsed. |
|  **mqttUrl** | The URL of the MQTT broker.  |

