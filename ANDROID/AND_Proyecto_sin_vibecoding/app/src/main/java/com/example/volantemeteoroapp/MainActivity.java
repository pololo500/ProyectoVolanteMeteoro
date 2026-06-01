package com.example.volantemeteoroapp;

import android.os.Bundle;
import android.content.Intent;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.json.JSONObject;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.example.volantemeteoroapp.mqtt.MqttConfig;
import com.example.volantemeteoroapp.mqtt.MqttManager;

public class MainActivity extends AppCompatActivity {

    private MqttManager mqttManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        EditText editTextUmbralMano = findViewById(R.id.edit_text_umbral_mano); // Campo para el umbral de mano
        EditText editTextUmbralVolanteLeve = findViewById(R.id.edit_text_umbral_volante_leve);
        EditText editTextUmbralVolanteBrusco = findViewById(R.id.edit_text_umbral_volante_brusco);

        // 1. Cargar valores desde la memoria interna (SharedPreferences)
        android.content.SharedPreferences prefs = getSharedPreferences("VolanteMeteoroPrefs", android.content.Context.MODE_PRIVATE);
        editTextUmbralMano.setText(prefs.getString("umbralMano", "1366"));
        editTextUmbralVolanteLeve.setText(prefs.getString("umbralLeve", "80"));
        editTextUmbralVolanteBrusco.setText(prefs.getString("umbralBrusco", "260"));
        Button buttonUmbrales = findViewById(R.id.button_umbrales); // Boton que toma los valores de la pantalla
        Button buttonAlarm = findViewById(R.id.button_alarm); // Boton para pedir la alarma
        Button buttonGoToSecond = findViewById(R.id.button_go_to_second);
        TextView textStatus = findViewById(R.id.text_status); // Texto donde se muestra el resultado

        // TextViews de la card de monitoreo en tiempo real
        TextView textEstadoFsm = findViewById(R.id.text_estado_fsm);
        TextView textFsrIzq = findViewById(R.id.text_fsr_izq);
        TextView textFsrDer = findViewById(R.id.text_fsr_der);
        TextView textVolante = findViewById(R.id.text_volante);

        mqttManager = new MqttManager(new MqttManager.Listener() {
            @Override
            public void onConnected() {
                mqttManager.subscribe(MqttConfig.TOPIC_ESTADO); // Suscribimos al estado de la ESP32
                mqttManager.subscribe(MqttConfig.TOPIC_SENSORES); // Suscribimos a los sensores
                runOnUiThread(() -> textStatus.setText("MQTT conectado. Suscripto a estado del ESP32."));
            }

            @Override
            public void onDisconnected() {
                runOnUiThread(() -> textStatus.setText("MQTT desconectado."));
            }

            @Override
            public void onConnectionError(String message) {
                runOnUiThread(() -> textStatus.setText("Error MQTT: " + message));
            }

            @Override
            public void onMessageReceived(String topic, String message) {
                runOnUiThread(() -> {
                    if (topic.equals(MqttConfig.TOPIC_ESTADO)) {
                        textEstadoFsm.setText(message);
                    } else if (topic.equals(MqttConfig.TOPIC_SENSORES)) {
                        try {
                            JSONObject json = new JSONObject(message);
                            textFsrIzq.setText(String.valueOf(json.getInt("fsrIzq")));
                            textFsrDer.setText(String.valueOf(json.getInt("fsrDer")));
                            textVolante.setText(String.valueOf(json.getInt("volante")));
                            textEstadoFsm.setText(json.getString("estado"));
                        } catch (Exception e) {
                            textStatus.setText("Error al parsear sensores: " + e.getMessage());
                        }
                    }
                });
            }
        });
        mqttManager.connect();

        buttonUmbrales.setOnClickListener(v -> { // Se ejecuta cuando el usuario toca el boton
            String umbralMano = editTextUmbralMano.getText().toString().trim();
            String umbralLeve = editTextUmbralVolanteLeve.getText().toString().trim();
            String umbralBrusco = editTextUmbralVolanteBrusco.getText().toString().trim();

            // Si falta algun dato, no seguimos
            if (umbralMano.isEmpty() || umbralLeve.isEmpty() || umbralBrusco.isEmpty()) {
                textStatus.setText("Error: completa los tres umbrales antes de continuar.");
                return;
            }

            try {
                // Convertimos el texto escrito por el usuario a numeros enteros
                int valorUmbralMano = Integer.parseInt(umbralMano);
                int valorUmbralLeve = Integer.parseInt(umbralLeve);
                int valorUmbralBrusco = Integer.parseInt(umbralBrusco);

                String mensaje = "Umbral mano: " + valorUmbralMano
                        + "\nUmbral volante leve: " + valorUmbralLeve
                        + "\nUmbral volante brusco: " + valorUmbralBrusco;

                textStatus.setText("Umbrales guardados correctamente.\n" + mensaje); // Mostramos el mensajes de status

                // Publicamos los 3 umbrales por MQTT al ESP32
                mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "UMBRAL_MANO:" + valorUmbralMano);
                mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "UMBRAL_LEVE:" + valorUmbralLeve);
                mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "UMBRAL_BRUSCO:" + valorUmbralBrusco);

                // 2. Guardar los nuevos valores en la memoria interna
                android.content.SharedPreferences.Editor editor = prefs.edit();
                editor.putString("umbralMano", umbralMano);
                editor.putString("umbralLeve", umbralLeve);
                editor.putString("umbralBrusco", umbralBrusco);
                editor.apply();

                // Sacamos el foco y ocultamos el teclado
                editTextUmbralMano.clearFocus();
                editTextUmbralVolanteLeve.clearFocus();
                editTextUmbralVolanteBrusco.clearFocus();
                android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                }
            } catch (NumberFormatException exception) {
                // Si escriben letras o simbolos, avisamos el error
                textStatus.setText("Error: escribi solo numeros enteros en los tres campos.");
            }
        });

        buttonAlarm.setOnClickListener(v -> { // Accion manual para disparar la alarma
            mqttManager.publish(MqttConfig.TOPIC_COMANDOS, "ALARMA");
            textStatus.setText("Alarma enviada al ESP32.");
        });

        buttonGoToSecond.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, SecondActivity.class);
            startActivity(intent);
        });

        int paddingHorizontal = (int) (24 * getResources().getDisplayMetrics().density);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.ime());
            // Mantenemos 24dp a los costados, pero dejamos que el teléfono maneje arriba y abajo
            v.setPadding(paddingHorizontal, systemBars.top, paddingHorizontal, systemBars.bottom);
            return insets;
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mqttManager != null) {
            mqttManager.disconnect();
        }
    }
}
