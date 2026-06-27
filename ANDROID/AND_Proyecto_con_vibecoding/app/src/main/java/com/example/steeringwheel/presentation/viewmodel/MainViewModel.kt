package com.example.steeringwheel.presentation.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.steeringwheel.data.repository.SteeringWheelRepositoryImpl
import com.example.steeringwheel.domain.model.MqttSettings
import com.example.steeringwheel.domain.model.SystemStatus
import com.example.steeringwheel.domain.repository.SettingsRepository
import com.example.steeringwheel.domain.repository.SteeringWheelRepository
import com.example.steeringwheel.presentation.state.DashboardUiState
import com.example.steeringwheel.presentation.state.SettingsUiState
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class MainViewModel @Inject constructor(
    private val steeringWheelRepository: SteeringWheelRepository,
    private val settingsRepository: SettingsRepository
) : ViewModel() {

    private val _dashboardState = MutableStateFlow(DashboardUiState())
    val dashboardState: StateFlow<DashboardUiState> = _dashboardState.asStateFlow()

    private val _settingsState = MutableStateFlow(SettingsUiState())
    val settingsState: StateFlow<SettingsUiState> = _settingsState.asStateFlow()

    init {
        observeData()
        loadSettings()
    }

    private fun observeData() {
        combine(
            steeringWheelRepository.sensorData,
            steeringWheelRepository.connectionStatus,
            steeringWheelRepository.lastError,
            (steeringWheelRepository as SteeringWheelRepositoryImpl).logs
        ) { data, connected, error, log ->
            _dashboardState.update { 
                it.copy(
                    sensorData = data,
                    isConnected = connected,
                    systemStatus = SystemStatus.fromString(data.estado),
                    lastError = error,
                    logs = (it.logs + log).takeLast(50)
                )
            }
        }.launchIn(viewModelScope)
    }

    private fun loadSettings() {
        settingsRepository.getMqttSettings()
            .onEach { settings ->
                _settingsState.update { it.copy(mqttSettings = settings) }
            }.launchIn(viewModelScope)
    }

    fun connect() {
        viewModelScope.launch {
            steeringWheelRepository.connect()
        }
    }

    fun disconnect() {
        viewModelScope.launch {
            steeringWheelRepository.disconnect()
        }
    }

    fun sendCommand(command: String) {
        viewModelScope.launch {
            steeringWheelRepository.sendCommand(command)
        }
    }

    fun updateSettings(settings: MqttSettings) {
        viewModelScope.launch {
            settingsRepository.updateMqttSettings(settings)
        }
    }
    
    fun setThresholds(hand: Int, light: Int, sharp: Int) {
        viewModelScope.launch {
            steeringWheelRepository.setThresholds(hand, light, sharp)
        }
    }
}
