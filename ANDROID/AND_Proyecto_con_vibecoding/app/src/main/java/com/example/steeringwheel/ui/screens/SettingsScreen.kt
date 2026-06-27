package com.example.steeringwheel.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.example.steeringwheel.domain.model.MqttSettings
import com.example.steeringwheel.presentation.state.SettingsUiState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    state: SettingsUiState,
    onSave: (MqttSettings) -> Unit
) {
    var broker by remember(state.mqttSettings.broker) { mutableStateOf(state.mqttSettings.broker) }
    var port by remember(state.mqttSettings.port) { mutableStateOf(state.mqttSettings.port.toString()) }
    var clientId by remember(state.mqttSettings.clientId) { mutableStateOf(state.mqttSettings.clientId) }

    Scaffold(
        topBar = { TopAppBar(title = { Text("Configuración MQTT") }) },
        floatingActionButton = {
            FloatingActionButton(onClick = { 
                onSave(MqttSettings(broker, port.toIntOrNull() ?: 1883, clientId))
            }) {
                Icon(Icons.Default.Save, contentDescription = "Guardar")
            }
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .padding(16.dp)
                .fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            OutlinedTextField(
                value = broker,
                onValueChange = { broker = it },
                label = { Text("Broker URL") },
                modifier = Modifier.fillMaxWidth()
            )
            OutlinedTextField(
                value = port,
                onValueChange = { port = it },
                label = { Text("Puerto") },
                modifier = Modifier.fillMaxWidth()
            )
            OutlinedTextField(
                value = clientId,
                onValueChange = { clientId = it },
                label = { Text("Client ID") },
                modifier = Modifier.fillMaxWidth()
            )
            
            Text(
                "Nota: Cambiar estos valores requerirá una reconexión manual desde el Dashboard.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}
