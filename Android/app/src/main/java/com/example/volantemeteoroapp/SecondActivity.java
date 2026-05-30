package com.example.volantemeteoroapp;

import android.os.Bundle;
import android.widget.Button;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public class SecondActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_second);

        Button buttonBack = findViewById(R.id.button_back);
        buttonBack.setOnClickListener(v -> {
            finish(); // Cierra esta pantalla y vuelve a la anterior
        });

        // 1. Manejo del teclado y los bordes físicos del teléfono
        android.view.View root = findViewById(android.R.id.content);
        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            // Pedimos el espacio de la barra superior/inferior + el teclado (ime)
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.ime());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        // 2. Lógica para la caja de texto y la memoria interna
        Button buttonSave = findViewById(R.id.button_save_speed_threshold);
        android.widget.EditText editTextThreshold = findViewById(R.id.edit_text_speed_threshold);

        android.content.SharedPreferences prefs = getSharedPreferences("VolanteMeteoroPrefs", android.content.Context.MODE_PRIVATE);
        editTextThreshold.setText(prefs.getString("umbralVelocidad", "10")); // 10 km/h por defecto

        buttonSave.setOnClickListener(v -> {
            // Guardar el número nuevo en la memoria
            String umbral = editTextThreshold.getText().toString().trim();
            if (!umbral.isEmpty()) {
                prefs.edit().putString("umbralVelocidad", umbral).apply();
            }

            // Le sacamos el foco a la caja de texto para que deje de titilar
            editTextThreshold.clearFocus();
            // Escondemos el teclado de la pantalla
            android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
            }
        });
    }
}
