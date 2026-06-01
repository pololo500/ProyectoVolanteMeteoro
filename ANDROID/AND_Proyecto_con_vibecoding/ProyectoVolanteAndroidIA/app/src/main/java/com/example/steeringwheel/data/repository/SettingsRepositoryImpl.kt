package com.example.steeringwheel.data.repository

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.example.steeringwheel.domain.model.MqttSettings
import com.example.steeringwheel.domain.repository.SettingsRepository
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

private val Context.dataStore by preferencesDataStore(name = "settings")

@Singleton
class SettingsRepositoryImpl @Inject constructor(
    @ApplicationContext private val context: Context
) : SettingsRepository {

    private object PreferencesKeys {
        val BROKER = stringPreferencesKey("mqtt_broker")
        val PORT = intPreferencesKey("mqtt_port")
        val CLIENT_ID = stringPreferencesKey("mqtt_client_id")
    }

    override fun getMqttSettings(): Flow<MqttSettings> {
        return context.dataStore.data.map { preferences ->
            MqttSettings(
                broker = preferences[PreferencesKeys.BROKER] ?: "broker.emqx.io",
                port = preferences[PreferencesKeys.PORT] ?: 1883,
                clientId = preferences[PreferencesKeys.CLIENT_ID] ?: "android_steering_wheel_${System.currentTimeMillis()}"
            )
        }
    }

    override suspend fun updateMqttSettings(settings: MqttSettings) {
        context.dataStore.edit { preferences ->
            preferences[PreferencesKeys.BROKER] = settings.broker
            preferences[PreferencesKeys.PORT] = settings.port
            preferences[PreferencesKeys.CLIENT_ID] = settings.clientId
        }
    }
}
