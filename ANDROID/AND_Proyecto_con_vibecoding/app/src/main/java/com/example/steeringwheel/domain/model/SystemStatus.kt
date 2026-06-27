package com.example.steeringwheel.domain.model

enum class SystemStatus {
    ST_INIT,
    ST_DETECTANDO,
    ST_ALERTA_LEVE,
    ST_ALERTA_FUERTE,
    ST_ALARMA_CELULAR,
    ST_ERROR,
    UNKNOWN;

    companion object {
        fun fromString(value: String): SystemStatus {
            return entries.find { it.name == value } ?: UNKNOWN
        }
    }
}
