package com.example.steeringwheel.presentation.state

import com.example.steeringwheel.domain.model.MqttSettings
import com.example.steeringwheel.domain.model.SensorData
import com.example.steeringwheel.domain.model.SystemStatus

data class DashboardUiState(
    val isConnected: Boolean = false,
    val sensorData: SensorData = SensorData(),
    val systemStatus: SystemStatus = SystemStatus.ST_INIT,
    val lastError: String? = null,
    val logs: List<String> = emptyList()
)

data class SettingsUiState(
    val mqttSettings: MqttSettings = MqttSettings(),
    val isSaving: Boolean = false
)
