package com.example.volantemeteoroapp.mqtt;

public final class MqttConfig {
    public static final String BROKER_URI = "tcp://broker.emqx.io:1883";
    public static final String CLIENT_ID = "VolanteMeteoroAppAndroid";
    public static final String TOPIC_COMANDOS = "volante/comandos";
    public static final String TOPIC_ESTADO = "volante/estado";
    public static final String TOPIC_SENSORES = "volante/sensores";

    private MqttConfig() {
    }
}
