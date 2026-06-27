package com.example.steeringwheel.domain.repository

import com.example.steeringwheel.domain.model.SensorData
import com.example.steeringwheel.domain.model.SystemStatus
import kotlinx.coroutines.flow.Flow

interface SteeringWheelRepository {
    val sensorData: Flow<SensorData>
    val connectionStatus: Flow<Boolean>
    val lastError: Flow<String?>

    suspend fun connect()
    suspend fun disconnect()
    suspend fun sendCommand(command: String)
    suspend fun setThresholds(hand: Int, light: Int, sharp: Int)
}
