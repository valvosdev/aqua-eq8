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
mosquitto_sub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001001/zone/" \
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


mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001003/zone/2/set" -d  \
  -m "1" \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure

mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001003/master/set" \
  -m "0" \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure

 mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001001/schedule/0/set" \
  -m '{"name":"Daytime Run","is_active":true,"steps":[{"zone":1,"time":420},{"zone":2,"time":600},{"zone":3,"time":420},{"zone":4,"time":600}]}' \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure


  mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001001/schedule/1/set" \
  -m '{"name":"Quick Flush","is_active":true,"interval":10,"steps":[{"zone":1,"time":30},{"zone":2,"time":30},{"zone":3,"time":30},{"zone":4,"time":30}]}' \
  --cafile ./certs/valvos-dev-ca.crt \
  --cert ./certs/client.crt \
  --key ./certs/client.key \
  --insecure

  mosquitto_pub -h 10.30.10.30 -p 8883 -t "tenant/0001000/device/1001001/schedule/1/start" -m "" \
  --cafile ./certs/valvos-dev-ca.crt --cert ./certs/client.crt --key ./certs/client.key --insecure