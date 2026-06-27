package com.example.steeringwheel.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.example.steeringwheel.presentation.state.DashboardUiState
import com.example.steeringwheel.ui.components.SensorCard
import com.example.steeringwheel.ui.components.StatusIndicator

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(
    state: DashboardUiState,
    onSendCommand: (String) -> Unit,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Dashboard Volante") },
                actions = {
                    IconButton(onClick = if (state.isConnected) onDisconnect else onConnect) {
                        Icon(
                            imageVector = if (state.isConnected) Icons.Default.CloudDone else Icons.Default.CloudOff,
                            contentDescription = "Conexión",
                            tint = if (state.isConnected) Color.Green else MaterialTheme.colorScheme.error
                        )
                    }
                }
            )
        }
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("Estado del Sistema", style = MaterialTheme.typography.titleMedium)
                    StatusIndicator(status = state.systemStatus)
                }
            }

            item {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Row(modifier = Modifier.fillMaxWidth()) {
                        SensorCard(
                            title = "FSR Izquierdo",
                            value = state.sensorData.fsrIzq.toString(),
                            unit = "N",
                            icon = Icons.Default.TouchApp,
                            modifier = Modifier.weight(1f)
                        )
                        SensorCard(
                            title = "FSR Derecho",
                            value = state.sensorData.fsrDer.toString(),
                            unit = "N",
                            icon = Icons.Default.TouchApp,
                            modifier = Modifier.weight(1f)
                        )
                    }
                    SensorCard(
                        title = "Ángulo Volante",
                        value = state.sensorData.volante.toString(),
                        unit = "°",
                        icon = Icons.Default.Explore,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
            }

            item {
                Text("Comandos Rápidos", style = MaterialTheme.typography.titleMedium)
                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Button(
                        onClick = { onSendCommand("ALARMA") },
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(Icons.Default.NotificationsActive, contentDescription = null)
                        Spacer(Modifier.width(8.dp))
                        Text("ALARMA")
                    }
                }
            }

            item {
                Text("Ajustar Umbrales (Simulación)", style = MaterialTheme.typography.titleMedium)
                ThresholdControls(onThresholdsSet = { h, l, s -> 
                    onSendCommand("UMBRAL_MANO:$h")
                    onSendCommand("UMBRAL_LEVE:$l")
                    onSendCommand("UMBRAL_BRUSCO:$s")
                })
            }
        }
    }
}

@Composable
fun ThresholdControls(onThresholdsSet: (Int, Int, Int) -> Unit) {
    var hand by remember { mutableStateOf(50f) }
    var light by remember { mutableStateOf(100f) }
    var sharp by remember { mutableStateOf(200f) }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f))
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Mano: ${hand.toInt()}")
            Slider(value = hand, onValueChange = { hand = it }, valueRange = 0f..1023f)
            
            Text("Leve: ${light.toInt()}")
            Slider(value = light, onValueChange = { light = it }, valueRange = 0f..1023f)
            
            Text("Brusco: ${sharp.toInt()}")
            Slider(value = sharp, onValueChange = { sharp = it }, valueRange = 0f..1023f)
            
            Button(
                onClick = { onThresholdsSet(hand.toInt(), light.toInt(), sharp.toInt()) },
                modifier = Modifier.align(Alignment.End)
            ) {
                Text("Enviar Umbrales")
            }
        }
    }
}
