package com.example.steeringwheel.domain.model

data class MqttSettings(
    val broker: String = "broker.emqx.io",
    val port: Int = 1883,
    val clientId: String = "android_steering_wheel_${System.currentTimeMillis()}"
)
