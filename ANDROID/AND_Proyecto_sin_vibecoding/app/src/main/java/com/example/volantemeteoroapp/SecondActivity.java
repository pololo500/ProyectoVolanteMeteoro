package com.example.volantemeteoroapp;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.location.Location;
import android.os.Bundle;
import android.os.Looper;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.example.volantemeteoroapp.mqtt.MqttConfig;
import com.example.volantemeteoroapp.mqtt.MqttManager;
import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.LocationCallback;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationResult;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.location.Priority;

import java.util.Locale;

public class SecondActivity extends AppCompatActivity {

    // === GPS ===
    private FusedLocationProviderClient fusedLocationClient;
    private LocationCallback locationCallback;
    private Location lastLocation = null;
    private long lastLocationTime = 0;
    private boolean gpsActivo = false;

    // === MQTT ===
    private MqttManager mqttManager;
    private boolean mqttConectado = false;

    // === Umbral ===
    private int umbralVelocidad = 10; // km/h por defecto

    // === Envío periódico de velocidad ===
    private static final long INTERVALO_ENVIO_VELOCIDAD_MS = 2000; // cada 2 segundos
    private long ultimoEnvioVelocidad = 0;
    private int ultimaVelocidadEnviada = -1;

    // === UI ===
    private TextView textSpeedValue;
    private TextView textSpeedStatusIndicator;
    private TextView textGpsStatus;
    private TextView textLatitude;
    private TextView textLongitude;
    private TextView textAccuracy;

    // Launcher para pedir permiso de ubicación en runtime
    private final ActivityResultLauncher<String> requestPermissionLauncher =
            registerForActivityResult(new ActivityResultContracts.RequestPermission(), isGranted -> {
                if (isGranted) {
                    iniciarGps();
                } else {
                    textGpsStatus.setText(R.string.gps_status_denied);
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_second);

        // --- Referencias a vistas ---
        textSpeedValue = findViewById(R.id.text_speed_value);
        textSpeedStatusIndicator = findViewById(R.id.text_speed_status_indicator);
        textGpsStatus = findViewById(R.id.text_gps_status);
        textLatitude = findViewById(R.id.text_latitude);
        textLongitude = findViewById(R.id.text_longitude);
        textAccuracy = findViewById(R.id.text_accuracy);
        Button buttonBack = findViewById(R.id.button_back);
        Button buttonSave = findViewById(R.id.button_save_speed_threshold);
        EditText editTextThreshold = findViewById(R.id.edit_text_speed_threshold);

        // --- Cargar umbral guardado ---
        android.content.SharedPreferences prefs = getSharedPreferences("VolanteMeteoroPrefs", MODE_PRIVATE);
        umbralVelocidad = Integer.parseInt(prefs.getString("umbralVelocidad", "10"));
        editTextThreshold.setText(String.valueOf(umbralVelocidad));

        // --- Botón volver ---
        buttonBack.setOnClickListener(v -> finish());

        // --- Botón guardar umbral ---
        buttonSave.setOnClickListener(v -> {
            String textoUmbral = editTextThreshold.getText().toString().trim();
            if (textoUmbral.isEmpty()) {
                textGpsStatus.setText("Error: ingresá un valor de umbral.");
                return;
            }

            try {
                int nuevoUmbral = Integer.parseInt(textoUmbral);
                umbralVelocidad = nuevoUmbral;

                // Guardar en SharedPreferences
                prefs.edit().putString("umbralVelocidad", String.valueOf(nuevoUmbral)).apply();

                // Enviar por MQTT al ESP32
                if (mqttManager != null && mqttConectado) {
                    mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "UMBRAL_VELOCIDAD:" + nuevoUmbral);
                    textGpsStatus.setText("Umbral guardado y enviado al volante: " + nuevoUmbral + " km/h");
                } else {
                    textGpsStatus.setText("Umbral guardado. Sin conexión MQTT para enviar.");
                }

                // Actualizar indicador visual
                actualizarIndicadorVelocidad(obtenerVelocidadActual());

                // Ocultar teclado
                editTextThreshold.clearFocus();
                android.view.inputmethod.InputMethodManager imm =
                        (android.view.inputmethod.InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                }
            } catch (NumberFormatException e) {
                textGpsStatus.setText("Error: ingresá solo números enteros.");
            }
        });

        // --- Insets para bordes del teléfono ---
        android.view.View root = findViewById(android.R.id.content);
        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.ime());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        // --- Inicializar MQTT ---
        inicializarMqtt();

        // --- Inicializar GPS ---
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this);
        crearLocationCallback();

        // Verificar permisos y arrancar
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                == PackageManager.PERMISSION_GRANTED) {
            iniciarGps();
        } else {
            requestPermissionLauncher.launch(Manifest.permission.ACCESS_FINE_LOCATION);
        }
    }

    // ========================================================================
    // GPS
    // ========================================================================

    private void crearLocationCallback() {
        locationCallback = new LocationCallback() {
            @Override
            public void onLocationResult(LocationResult locationResult) {
                if (locationResult == null) return;

                Location location = locationResult.getLastLocation();
                if (location == null) return;

                procesarNuevaUbicacion(location);
            }
        };
    }

    @SuppressLint("MissingPermission")
    private void iniciarGps() {
        gpsActivo = true;
        textGpsStatus.setText(R.string.gps_status_searching);

        // Configurar las actualizaciones de ubicación: cada 1 segundo, prioridad alta
        LocationRequest locationRequest = new LocationRequest.Builder(
                Priority.PRIORITY_HIGH_ACCURACY, 1000)
                .setMinUpdateIntervalMillis(500)
                .setWaitForAccurateLocation(false)
                .build();

        fusedLocationClient.requestLocationUpdates(locationRequest, locationCallback, Looper.getMainLooper());
    }

    private void detenerGps() {
        if (fusedLocationClient != null && locationCallback != null) {
            fusedLocationClient.removeLocationUpdates(locationCallback);
        }
        gpsActivo = false;
    }

    private void procesarNuevaUbicacion(Location location) {
        // Actualizar info GPS en pantalla
        textLatitude.setText(String.format(Locale.getDefault(), "%.6f", location.getLatitude()));
        textLongitude.setText(String.format(Locale.getDefault(), "%.6f", location.getLongitude()));

        if (location.hasAccuracy()) {
            textAccuracy.setText(String.format(Locale.getDefault(), "%.1f m", location.getAccuracy()));
        }

        // Calcular velocidad
        int velocidadKmH = calcularVelocidad(location);

        // Actualizar velocidad en pantalla
        textSpeedValue.setText(String.valueOf(velocidadKmH));

        // Actualizar indicador de estado
        actualizarIndicadorVelocidad(velocidadKmH);

        // Actualizar estado GPS
        if (mqttConectado) {
            textGpsStatus.setText(R.string.gps_status_mqtt_connected);
        } else {
            textGpsStatus.setText(R.string.gps_status_active);
        }

        // Enviar velocidad por MQTT (cada INTERVALO_ENVIO_VELOCIDAD_MS)
        long ahora = System.currentTimeMillis();
        if (ahora - ultimoEnvioVelocidad >= INTERVALO_ENVIO_VELOCIDAD_MS) {
            enviarVelocidadMqtt(velocidadKmH);
            ultimoEnvioVelocidad = ahora;
        }

        // Guardar para próximo cálculo
        lastLocation = location;
        lastLocationTime = location.getTime();
    }

    /**
     * Calcula la velocidad en km/h.
     * Si el GPS provee velocidad directa (hasSpeed), la usa.
     * Si no, calcula por diferencia de ubicación.
     */
    private int calcularVelocidad(Location location) {
        // Preferir la velocidad que reporta el GPS directamente (más precisa en movimiento)
        if (location.hasSpeed()) {
            float speedMs = location.getSpeed(); // metros por segundo
            return Math.round(speedMs * 3.6f);   // convertir a km/h
        }

        // Fallback: calcular por diferencia de ubicación
        if (lastLocation == null || lastLocationTime == 0) {
            return 0;
        }

        float distanciaMetros = lastLocation.distanceTo(location);
        long tiempoMs = location.getTime() - lastLocationTime;

        if (tiempoMs <= 0) {
            return 0;
        }

        float tiempoSegundos = tiempoMs / 1000.0f;
        float velocidadMs = distanciaMetros / tiempoSegundos;
        float velocidadKmH = velocidadMs * 3.6f;

        // Filtrar ruido GPS: velocidades muy bajas (< 1 km/h) se consideran 0
        if (velocidadKmH < 1.0f) {
            return 0;
        }

        return Math.round(velocidadKmH);
    }

    // ========================================================================
    // Indicador visual de velocidad
    // ========================================================================

    private void actualizarIndicadorVelocidad(int velocidadKmH) {
        if (velocidadKmH >= umbralVelocidad) {
            // Velocidad mayor o igual al umbral → las maniobras sinuosas SÍ activan alarma
            textSpeedStatusIndicator.setText(R.string.speed_above_threshold);
            textSpeedStatusIndicator.setTextColor(ContextCompat.getColor(this, R.color.speed_safe));
            textSpeedStatusIndicator.setBackgroundResource(R.drawable.rounded_indicator_safe_bg);
        } else {
            // Velocidad menor al umbral → maniobras silenciadas (está doblando)
            textSpeedStatusIndicator.setText(R.string.speed_below_threshold);
            textSpeedStatusIndicator.setTextColor(ContextCompat.getColor(this, R.color.speed_warning));
            textSpeedStatusIndicator.setBackgroundResource(R.drawable.rounded_indicator_bg);
        }
    }

    private int obtenerVelocidadActual() {
        try {
            return Integer.parseInt(textSpeedValue.getText().toString());
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    // ========================================================================
    // MQTT
    // ========================================================================

    private void inicializarMqtt() {
        mqttManager = new MqttManager(new MqttManager.Listener() {
            @Override
            public void onConnected() {
                mqttConectado = true;
                runOnUiThread(() -> {
                    if (gpsActivo) {
                        textGpsStatus.setText(R.string.gps_status_mqtt_connected);
                    }
                });
                // Enviar el umbral actual al conectar para sincronizar con el ESP32
                mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "UMBRAL_VELOCIDAD:" + umbralVelocidad);
            }

            @Override
            public void onDisconnected() {
                mqttConectado = false;
                runOnUiThread(() -> {
                    if (gpsActivo) {
                        textGpsStatus.setText(R.string.gps_status_active);
                    }
                });
            }

            @Override
            public void onConnectionError(String message) {
                mqttConectado = false;
                runOnUiThread(() -> textGpsStatus.setText(R.string.gps_status_mqtt_error));
            }

            @Override
            public void onMessageReceived(String topic, String message) {
                // Esta pantalla no necesita recibir mensajes, solo envía
            }
        });
        mqttManager.connect();
    }

    private void enviarVelocidadMqtt(int velocidadKmH) {
        if (mqttManager != null && mqttConectado) {
            // Solo enviar si la velocidad cambió (evitar mensajes redundantes)
            if (velocidadKmH != ultimaVelocidadEnviada) {
                mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "VELOCIDAD:" + velocidadKmH);
                ultimaVelocidadEnviada = velocidadKmH;
            }
        }
    }

    // ========================================================================
    // Ciclo de vida
    // ========================================================================

    @Override
    protected void onDestroy() {
        super.onDestroy();
        detenerGps();
        if (mqttManager != null) {
            mqttManager.disconnect();
        }
    }
}
