# HbGateway
Provides a link between MQTT and the HameBoss databse.

## Configuration
The default configuration file is called, *HbGateway.config.json*, in the current folder unless another folder has been specified in the command-line arguments.

| Parameter | Description |
|-----------|-------------|
| **topicRoot** | The root name of the MQTT topic to be monitored.  Messages in any topic beginning with this will be processsed. |
|  **mqttUrl** | The URL of the MQTT broker.  |

