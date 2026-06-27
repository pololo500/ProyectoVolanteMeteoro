package com.example.steeringwheel.domain.model

import kotlinx.serialization.Serializable

@Serializable
data class SensorData(
    val fsrIzq: Float = 0f,
    val fsrDer: Float = 0f,
    val volante: Float = 0f,
    val timestamp: Long = 0L,
    val estado: String = "ST_INIT"
)
