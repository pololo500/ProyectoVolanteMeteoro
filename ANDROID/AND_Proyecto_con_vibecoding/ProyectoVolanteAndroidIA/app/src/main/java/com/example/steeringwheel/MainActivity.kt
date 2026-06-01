package com.example.steeringwheel

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.example.steeringwheel.presentation.viewmodel.MainViewModel
import com.example.steeringwheel.ui.screens.DashboardScreen
import com.example.steeringwheel.ui.screens.LogScreen
import com.example.steeringwheel.ui.screens.SettingsScreen
import com.example.steeringwheel.ui.theme.SteeringWheelTheme
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SteeringWheelTheme {
                MainScreen()
            }
        }
    }
}

sealed class Screen(val route: String, val label: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    object Dashboard : Screen("dashboard", "Dashboard", Icons.Default.Dashboard)
    object Logs : Screen("logs", "Logs", Icons.Default.History)
    object Settings : Screen("settings", "Ajustes", Icons.Default.Settings)
}

@Composable
fun MainScreen() {
    val navController = rememberNavController()
    val viewModel: MainViewModel = hiltViewModel()
    
    val items = listOf(Screen.Dashboard, Screen.Logs, Screen.Settings)

    Scaffold(
        bottomBar = {
            NavigationBar {
                val navBackStackEntry by navController.currentBackStackEntryAsState()
                val currentDestination = navBackStackEntry?.destination
                items.forEach { screen ->
                    NavigationBarItem(
                        icon = { Icon(screen.icon, contentDescription = null) },
                        label = { Text(screen.label) },
                        selected = currentDestination?.hierarchy?.any { it.route == screen.route } == true,
                        onClick = {
                            navController.navigate(screen.route) {
                                popUpTo(navController.graph.findStartDestination().id) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(navController, startDestination = Screen.Dashboard.route, Modifier.padding(innerPadding)) {
            composable(Screen.Dashboard.route) {
                val state by viewModel.dashboardState.collectAsState()
                DashboardScreen(
                    state = state,
                    onSendCommand = viewModel::sendCommand,
                    onConnect = viewModel::connect,
                    onDisconnect = viewModel::disconnect
                )
            }
            composable(Screen.Logs.route) {
                val state by viewModel.dashboardState.collectAsState()
                LogScreen(logs = state.logs)
            }
            composable(Screen.Settings.route) {
                val state by viewModel.settingsState.collectAsState()
                SettingsScreen(
                    state = state,
                    onSave = viewModel::updateSettings
                )
            }
        }
    }
}
