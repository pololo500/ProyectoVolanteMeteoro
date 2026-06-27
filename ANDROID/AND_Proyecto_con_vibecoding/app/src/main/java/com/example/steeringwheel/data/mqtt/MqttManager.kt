package com.example.steeringwheel.data.mqtt

import android.util.Log
import com.example.steeringwheel.domain.model.MqttSettings
import com.example.steeringwheel.domain.model.SensorData
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt3.Mqtt3AsyncClient
import com.hivemq.client.mqtt.mqtt3.message.connect.connack.Mqtt3ConnAck
import com.hivemq.client.mqtt.mqtt3.message.publish.Mqtt3Publish
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.Json
import java.nio.charset.StandardCharsets
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MqttManager @Inject constructor() {

    private var client: Mqtt3AsyncClient? = null
    private val json = Json { ignoreUnknownKeys = true }

    private val _sensorData = MutableSharedFlow<SensorData>(replay = 1, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    val sensorData = _sensorData.asSharedFlow()

    private val _isConnected = MutableStateFlow(false)
    val isConnected = _isConnected.asStateFlow()

    private val _lastError = MutableStateFlow<String?>(null)
    val lastError = _lastError.asStateFlow()

    private val _logs = MutableSharedFlow<String>(replay = 10, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    val logs = _logs.asSharedFlow()

    fun connect(settings: MqttSettings) {
        if (_isConnected.value) return

        client = MqttClient.builder()
            .useMqttVersion3()
            .identifier(settings.clientId)
            .serverHost(settings.broker)
            .serverPort(settings.port)
            .automaticReconnectWithDefaultConfig()
            .addConnectedListener { _isConnected.value = true; addLog("Conectado al broker") }
            .addDisconnectedListener { _isConnected.value = false; addLog("Desconectado del broker") }
            .buildAsync()

        client?.connectWith()
            ?.send()
            ?.whenComplete { ack, throwable ->
                if (throwable != null) {
                    _lastError.value = throwable.message
                    addLog("Error de conexión: ${throwable.message}")
                } else {
                    subscribeToTopics()
                }
            }
    }

    private fun subscribeToTopics() {
        client?.subscribeWith()
            ?.topicFilter("volante/sensores")
            ?.callback { publish -> handleIncomingMessage(publish) }
            ?.send()

        client?.subscribeWith()
            ?.topicFilter("volante/estado")
            ?.callback { publish -> handleIncomingMessage(publish) }
            ?.send()
            
        addLog("Suscrito a tópicos")
    }

    private fun handleIncomingMessage(publish: Mqtt3Publish) {
        val payload = publish.payloadAsBytes
        val message = String(payload, StandardCharsets.UTF_8)
        
        when (publish.topic.toString()) {
            "volante/sensores", "volante/estado" -> {
                try {
                    val data = json.decodeFromString<SensorData>(message)
                    _sensorData.tryEmit(data)
                    addLog("Datos recibidos: $message")
                } catch (e: Exception) {
                    addLog("Error parseando JSON: ${e.message}")
                }
            }
        }
    }

    fun publish(topic: String, message: String) {
        client?.publishWith()
            ?.topic(topic)
            ?.payload(message.toByteArray(StandardCharsets.UTF_8))
            ?.send()
            ?.whenComplete { _, throwable ->
                if (throwable != null) {
                    addLog("Error publicando en $topic: ${throwable.message}")
                } else {
                    addLog("Publicado en $topic: $message")
                }
            }
    }

    fun disconnect() {
        client?.disconnect()
    }

    private fun addLog(message: String) {
        Log.d("MqttManager", message)
        _logs.tryEmit("${System.currentTimeMillis()}: $message")
    }
}
