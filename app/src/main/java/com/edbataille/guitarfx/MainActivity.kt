package com.edbataille.guitarfx

import android.Manifest
import android.content.ContentValues
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.MediaStore
import android.view.WindowManager
import android.widget.Button
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private external fun nativeStart(): Boolean
    private external fun nativeStop()
    private external fun nativeSetInputGain(v: Float)
    private external fun nativeSetVolume(v: Float)
    private external fun nativeSetDrive(v: Float)
    private external fun nativeSetTone(v: Float)
    private external fun nativeSetFx(v: Float)
    private external fun nativeSetMode(m: Int)
    private external fun nativeGetPeak(): Float
    private external fun nativeGetLatencyMs(): Double
    private external fun nativeStartRecording(path: String): Boolean
    private external fun nativeStopRecording()

    private var running = false
    private var recording = false
    private var mode = 0
    private var recStartMs = 0L

    private lateinit var btnPower: Button
    private lateinit var btnClean: Button
    private lateinit var btnDrive: Button
    private lateinit var btnMod: Button
    private lateinit var btnRec: Button
    private lateinit var meter: ProgressBar
    private lateinit var status: TextView

    private val tmpRecFile: File get() = File(cacheDir, "gravacao_tmp.wav")

    private val askPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) startEngine()
            else Toast.makeText(this,
                "Sem acesso ao microfone o som da guitarra não entra.",
                Toast.LENGTH_LONG).show()
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        btnPower = findViewById(R.id.btnPower)
        btnClean = findViewById(R.id.btnClean)
        btnDrive = findViewById(R.id.btnDrive)
        btnMod = findViewById(R.id.btnMod)
        btnRec = findViewById(R.id.btnRec)
        meter = findViewById(R.id.meter)
        status = findViewById(R.id.status)

        btnPower.setOnClickListener { if (running) stopEngine() else checkPermissionAndStart() }
        btnClean.setOnClickListener { setMode(0) }
        btnDrive.setOnClickListener { setMode(1) }
        btnMod.setOnClickListener { setMode(2) }
        btnRec.setOnClickListener { if (recording) stopRec() else startRec() }

        bindSlider(R.id.inGain, R.id.inGainLabel, "Ganho de entrada", 60) { nativeSetInputGain(it) }
        bindSlider(R.id.volume, R.id.volumeLabel, "Volume", 70) { nativeSetVolume(it) }
        bindSlider(R.id.drive, R.id.driveLabel, "Drive", 50) { nativeSetDrive(it) }
        bindSlider(R.id.tone, R.id.toneLabel, "Tom", 60) { nativeSetTone(it) }
        bindSlider(R.id.fx, R.id.fxLabel, "Delay / Chorus", 40) { nativeSetFx(it) }

        setMode(0)
        btnRec.isEnabled = false
        updateMeter()
    }

    private fun bindSlider(sliderId: Int, labelId: Int, name: String, initial: Int, onChange: (Float) -> Unit) {
        val slider = findViewById<SeekBar>(sliderId)
        val label = findViewById<TextView>(labelId)
        slider.progress = initial
        label.text = "$name  $initial"
        onChange(initial / 100f)
        slider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) {
                label.text = "$name  $p"; onChange(p / 100f)
            }
            override fun onStartTrackingTouch(s: SeekBar?) {}
            override fun onStopTrackingTouch(s: SeekBar?) {}
        })
    }

    private fun checkPermissionAndStart() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            == PackageManager.PERMISSION_GRANTED) startEngine()
        else askPermission.launch(Manifest.permission.RECORD_AUDIO)
    }

    private fun startEngine() {
        if (nativeStart()) {
            running = true
            btnPower.text = "POWER  ▸  ON"
            btnRec.isEnabled = true
            val lat = nativeGetLatencyMs()
            status.text = if (lat > 0) "Ligado — latência ~%.0f ms".format(lat)
                          else "Ligado — toque a guitarra"
        } else status.text = "Falha ao abrir o áudio. Fone/iRig conectados?"
    }

    private fun stopEngine() {
        if (recording) stopRec()
        nativeStop(); running = false
        btnPower.text = "POWER  ▸  STANDBY"; btnRec.isEnabled = false
        status.text = "Standby"; meter.progress = 0
    }

    private fun startRec() {
        tmpRecFile.delete()
        if (nativeStartRecording(tmpRecFile.absolutePath)) {
            recording = true; recStartMs = System.currentTimeMillis()
            btnRec.text = "■  STOP"; status.text = "● Gravando…"
        } else Toast.makeText(this, "Não consegui iniciar a gravação.", Toast.LENGTH_SHORT).show()
    }

    private fun stopRec() {
        nativeStopRecording(); recording = false
        btnRec.text = "●  REC"
        val secs = (System.currentTimeMillis() - recStartMs) / 1000
        val name = "GuitarFX_" + SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date()) + ".wav"
        val saved: String? = try {
            if (Build.VERSION.SDK_INT >= 29) {
                val values = ContentValues().apply {
                    put(MediaStore.Audio.Media.DISPLAY_NAME, name)
                    put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav")
                    put(MediaStore.Audio.Media.RELATIVE_PATH, Environment.DIRECTORY_MUSIC + "/GuitarFX")
                }
                val uri = contentResolver.insert(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, values)
                if (uri != null) {
                    contentResolver.openOutputStream(uri)?.use { out ->
                        tmpRecFile.inputStream().use { it.copyTo(out) } }
                    "Música/GuitarFX/$name"
                } else null
            } else {
                val dir = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
                val dst = File(dir, name); tmpRecFile.copyTo(dst, overwrite = true); dst.absolutePath
            }
        } catch (e: Exception) { null }
        tmpRecFile.delete()
        if (saved != null) {
            status.text = "Gravação de ${secs}s salva"
            Toast.makeText(this, "Salvo em $saved", Toast.LENGTH_LONG).show()
        } else status.text = "Erro ao salvar a gravação"
    }

    private fun setMode(m: Int) {
        mode = m
        nativeSetMode(m)
        btnClean.alpha = if (m == 0) 1f else 0.45f
        btnDrive.alpha = if (m == 1) 1f else 0.45f
        btnMod.alpha = if (m == 2) 1f else 0.45f
        findViewById<SeekBar>(R.id.drive).isEnabled = (m == 1)
        findViewById<SeekBar>(R.id.tone).isEnabled = (m == 1)
        findViewById<SeekBar>(R.id.fx).isEnabled = (m == 2)
    }

    private fun updateMeter() {
        meter.post(object : Runnable {
            override fun run() {
                if (running) {
                    meter.progress = (nativeGetPeak() * 130).toInt().coerceIn(0, 100)
                    if (recording) {
                        val secs = (System.currentTimeMillis() - recStartMs) / 1000
                        status.text = "● Gravando…  %d:%02d".format(secs / 60, secs % 60)
                    }
                } else meter.progress = 0
                meter.postDelayed(this, 60)
            }
        })
    }

    override fun onDestroy() {
        if (running) { if (recording) nativeStopRecording(); nativeStop() }
        super.onDestroy()
    }

    companion object { init { System.loadLibrary("guitarfx") } }
}
