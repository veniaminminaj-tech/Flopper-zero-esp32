package tech.veniaminminaj.flopperzero

import android.app.Activity
import android.os.Bundle
import android.graphics.Color
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.widget.*

class MainActivity : Activity() {
    private val bg = Color.rgb(7, 12, 20)
    private val panel = Color.rgb(16, 25, 38)
    private val cyan = Color.rgb(0, 225, 190)
    private lateinit var status: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(22, 28, 22, 20)
            setBackgroundColor(bg)
        }
        val scroll = ScrollView(this)
        val content = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val brand = TextView(this).apply {
            text = "F L O P P E R   Z E R O"
            textSize = 22f; setTextColor(cyan); typeface = Typeface.DEFAULT_BOLD
        }
        content.addView(brand)
        content.addView(label("CONTROL CENTER  /  ESP32", 11f, Color.LTGRAY, 0, 18))
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL; setPadding(18, 18, 18, 18)
            setBackgroundColor(panel)
        }
        card.addView(label("DEVICE STATUS", 12f, cyan, 0, 8))
        status = label("NOT CONNECTED", 20f, Color.WHITE, 0, 6)
        card.addView(status)
        card.addView(label("Connect your ESP32 using a USB-OTG cable.", 13f, Color.LTGRAY, 0, 0))
        content.addView(card, LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = 18 })
        content.addView(label("DASHBOARD", 12f, cyan, 0, 10))
        val grid = GridLayout(this).apply { columnCount = 2 }
        listOf("Connection", "Device info", "Serial monitor", "About").forEach { name ->
            val tile = TextView(this).apply {
                text = name.uppercase() + "\n\n—"
                textSize = 14f; setTextColor(Color.WHITE); gravity = Gravity.CENTER
                setPadding(12, 25, 12, 25); setBackgroundColor(panel)
            }
            val lp = GridLayout.LayoutParams().apply {
                width = 0; height = 130; columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f)
                setMargins(0, 0, 10, 10)
            }
            grid.addView(tile, lp)
        }
        content.addView(grid)
        content.addView(label("UI PREVIEW • Firmware files are not modified", 11f, Color.GRAY, 0, 12))
        scroll.addView(content)
        root.addView(scroll)
        setContentView(root)
    }

    private fun label(text: String, size: Float, color: Int, top: Int, bottom: Int): TextView =
        TextView(this).apply {
            this.text = text; textSize = size; setTextColor(color)
            setPadding(0, top, 0, bottom)
        }
}
