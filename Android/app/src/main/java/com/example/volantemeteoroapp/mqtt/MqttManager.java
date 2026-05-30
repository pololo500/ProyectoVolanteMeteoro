package com.example.volantemeteoroapp.mqtt;

import org.eclipse.paho.client.mqttv3.IMqttActionListener; // recibe aviso cuando una conexión o acción termina bien o mal.
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken; // representa el resultado de un mensaje enviado.
import org.eclipse.paho.client.mqttv3.IMqttToken; // representa una operación MQTT en curso.
import org.eclipse.paho.client.mqttv3.MqttAsyncClient; // el cliente MQTT que conecta y publica de forma asincronica
import org.eclipse.paho.client.mqttv3.MqttCallback; // define qué hacer cuando llega un mensaje o se pierde conexión.
import org.eclipse.paho.client.mqttv3.MqttClientPersistence; // guarda o maneja datos de sesión/conexión.
import org.eclipse.paho.client.mqttv3.MqttConnectOptions; // opciones para conectar al broker.
import org.eclipse.paho.client.mqttv3.MqttMessage; // el objeto que contiene el mensaje MQTT.
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence; // guardar datos temporales en memoria.

import java.nio.charset.StandardCharsets;

public class MqttManager {
    public interface Listener { // Avisos por pantalla
        void onConnected();

        void onDisconnected();

        void onConnectionError(String message);

        void onMessageReceived(String topic, String message);
    }

    private final Listener listener; // A quien avisar
    private final MqttAsyncClient client; // Cliente MQTT
    private final MqttConnectOptions connectOptions; // Opciones de conexion

    public MqttManager(Listener listener) { // Constructor
        this.listener = listener;
        try {
            MqttClientPersistence persistence = new MemoryPersistence();
            String uniqueClientId = MqttConfig.CLIENT_ID + "_" + System.currentTimeMillis();
            this.client = new MqttAsyncClient(MqttConfig.BROKER_URI, uniqueClientId, persistence);
            this.connectOptions = new MqttConnectOptions();
            this.connectOptions.setAutomaticReconnect(true);
            this.connectOptions.setCleanSession(true);
            this.connectOptions.setConnectionTimeout(10);
            this.connectOptions.setKeepAliveInterval(30);

            this.client.setCallback(new MqttCallback() {
                @Override
                public void connectionLost(Throwable cause) {
                    if (MqttManager.this.listener != null) {
                        MqttManager.this.listener.onConnectionError("Conexion perdida");
                    }
                }

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    if (MqttManager.this.listener != null) {
                        MqttManager.this.listener.onMessageReceived(topic, new String(message.getPayload(), StandardCharsets.UTF_8));
                    }
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {
                    // No necesitamos hacer nada aqui por ahora.
                }
            });
        } catch (Exception exception) {
            throw new IllegalStateException("No se pudo inicializar MQTT", exception);
        }
    }

    public boolean isConnected() {
        return client.isConnected();
    }

    public void connect() {
        if (client.isConnected()) {
            if (listener != null) {
                listener.onConnected();
            }
            return;
        }

        try {
            client.connect(connectOptions, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    if (listener != null) {
                        listener.onConnected();
                    }
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    if (listener != null) {
                        listener.onConnectionError(exception != null ? exception.getMessage() : "Error de conexion MQTT");
                    }
                }
            });
        } catch (Exception exception) {
            if (listener != null) {
                listener.onConnectionError(exception.getMessage());
            }
        }
    }

    public void disconnect() {
        if (!client.isConnected()) {
            if (listener != null) {
                listener.onDisconnected();
            }
            return;
        }

        try {
            client.disconnect().waitForCompletion();
            if (listener != null) {
                listener.onDisconnected();
            }
        } catch (Exception exception) {
            if (listener != null) {
                listener.onConnectionError(exception.getMessage());
            }
        }
    }

    public void publish(String topic, String payload) {
        if (!client.isConnected()) {
            if (listener != null) {
                listener.onConnectionError("No hay conexion MQTT activa");
            }
            return;
        }

        try {
            MqttMessage message = new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));
            message.setQos(0);
            client.publish(topic, message);
        } catch (Exception exception) {
            if (listener != null) {
                listener.onConnectionError(exception.getMessage());
            }
        }
    }

    public void subscribe(String topic) {
        if (!client.isConnected()) {
            if (listener != null) {
                listener.onConnectionError("No hay conexion MQTT activa");
            }
            return;
        }

        try {
            client.subscribe(topic, 0);
        } catch (Exception exception) {
            if (listener != null) {
                listener.onConnectionError(exception.getMessage());
            }
        }
    }
}
