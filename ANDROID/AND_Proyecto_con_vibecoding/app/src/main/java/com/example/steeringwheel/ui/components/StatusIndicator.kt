package com.example.steeringwheel.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.example.steeringwheel.domain.model.SystemStatus
import com.example.steeringwheel.ui.theme.*

@Composable
fun StatusIndicator(status: SystemStatus, modifier: Modifier = Modifier) {
    val color = when (status) {
        SystemStatus.ST_INIT -> ST_INIT_Color
        SystemStatus.ST_DETECTANDO -> ST_DETECTANDO_Color
        SystemStatus.ST_ALERTA_LEVE -> ST_ALERTA_LEVE_Color
        SystemStatus.ST_ALERTA_FUERTE -> ST_ALERTA_FUERTE_Color
        SystemStatus.ST_ALARMA_CELULAR -> ST_ALARMA_CELULAR_Color
        SystemStatus.ST_ERROR -> ST_ERROR_Color
        SystemStatus.UNKNOWN -> Color.Gray
    }

    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = modifier
            .clip(CircleShape)
            .background(color.copy(alpha = 0.1f))
            .padding(horizontal = 12.dp, vertical = 6.dp)
    ) {
        Box(
            modifier = Modifier
                .size(10.dp)
                .clip(CircleShape)
                .background(color)
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = status.name,
            style = MaterialTheme.typography.labelLarge,
            color = color,
            fontWeight = androidx.compose.ui.text.font.FontWeight.Bold
        )
    }
}
