package com.example.steeringwheel.data.repository

import com.example.steeringwheel.data.mqtt.MqttManager
import com.example.steeringwheel.domain.model.SensorData
import com.example.steeringwheel.domain.repository.SettingsRepository
import com.example.steeringwheel.domain.repository.SteeringWheelRepository
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class SteeringWheelRepositoryImpl @Inject constructor(
    private val mqttManager: MqttManager,
    private val settingsRepository: SettingsRepository
) : SteeringWheelRepository {

    override val sensorData: Flow<SensorData> = mqttManager.sensorData
    override val connectionStatus: Flow<Boolean> = mqttManager.isConnected
    override val lastError: Flow<String?> = mqttManager.lastError
    val logs: Flow<String> = mqttManager.logs

    override suspend fun connect() {
        val settings = settingsRepository.getMqttSettings().first()
        mqttManager.connect(settings)
    }

    override suspend fun disconnect() {
        mqttManager.disconnect()
    }

    override suspend fun sendCommand(command: String) {
        mqttManager.publish("volante/comandos", command)
    }

    override suspend fun setThresholds(hand: Int, light: Int, sharp: Int) {
        sendCommand("UMBRAL_MANO:$hand")
        sendCommand("UMBRAL_LEVE:$light")
        sendCommand("UMBRAL_BRUSCO:$sharp")
    }
}
