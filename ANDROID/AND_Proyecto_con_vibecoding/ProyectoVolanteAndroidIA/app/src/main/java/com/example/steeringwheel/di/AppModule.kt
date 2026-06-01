package com.example.steeringwheel.di

import android.content.Context
import com.example.steeringwheel.data.mqtt.MqttManager
import com.example.steeringwheel.data.repository.SettingsRepositoryImpl
import com.example.steeringwheel.data.repository.SteeringWheelRepositoryImpl
import com.example.steeringwheel.domain.repository.SettingsRepository
import com.example.steeringwheel.domain.repository.SteeringWheelRepository
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun provideSettingsRepository(
        @ApplicationContext context: Context
    ): SettingsRepository = SettingsRepositoryImpl(context)

    @Provides
    @Singleton
    fun provideSteeringWheelRepository(
        mqttManager: MqttManager,
        settingsRepository: SettingsRepository
    ): SteeringWheelRepository = SteeringWheelRepositoryImpl(mqttManager, settingsRepository)
}
