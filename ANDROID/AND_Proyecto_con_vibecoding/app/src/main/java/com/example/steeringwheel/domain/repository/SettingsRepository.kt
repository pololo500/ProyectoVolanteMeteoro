package com.example.steeringwheel.domain.repository

import com.example.steeringwheel.domain.model.MqttSettings
import kotlinx.coroutines.flow.Flow

interface SettingsRepository {
    fun getMqttSettings(): Flow<MqttSettings>
    suspend fun updateMqttSettings(settings: MqttSettings)
}
