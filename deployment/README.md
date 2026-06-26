# File file describe how to deploy the infrastructure on a kubernetes cluster.


``` kubectl create ns valvos ```

## MQTT

``` helm upgrade -i mqtt t3n/mosquitto --namespace valvos  -f ./mqtt/values.yaml ```