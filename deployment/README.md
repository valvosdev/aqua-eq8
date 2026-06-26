# File file describe how to deploy the infrastructure on a kubernetes cluster.


``` kubectl create ns valvos ```

## MQTT

Run ``` ./generate-cert-key.sh ``` first to generate self signed certificates 

Create secrets for certs files

```
kubectl create secret generic mqtt-tls-certs \
  --from-file=valvos-dev-ca.crt=./certs/valvos-dev-ca.crt \
  --from-file=server-crt.crt=./certs/server.crt \
  --from-file=server-crt.key=./certs/server.key \
  -n valvos
```

``` helm upgrade -i mqtt t3n/mosquitto --namespace valvos  -f ./mqtt/values.yaml ```

### Test connectivity:

Replace the IP address with yours

```
mosquitto_sub -h 10.30.10.30 -p 8883 -t "test/topic" \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure -v
```

Publish a message

```
 mosquitto_pub -h 10.30.10.30 -p 8883 -t "test/topic" \
  -m "Hello from macOS over MQTTS" \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure
  ```