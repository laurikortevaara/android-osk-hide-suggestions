/*
 * This is an example project for Android OSK hiding from native c++ via JNI call.
 */
package com.company.hideosksuggestions;

import android.support.annotation.Keep;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;
import com.company.hideosksuggestions.R;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViewById(R.id.button_toggle).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                toggleAndroidOskJNI();
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        initJNI();

        ((TextView)findViewById(R.id.editText1)).setText(stringFromJNI());
        ((TextView)findViewById(R.id.editText2)).setText(stringFromJNI());
    }

    @Override
    public void onPause () {
        super.onPause();
    }

    @Keep
    public void toggleTextEditInputType()
    {
        View focused = getCurrentFocus();
        if(focused instanceof EditText) {
            EditText etv = (EditText) focused;
            int inputtype = etv.getInputType();
            etv.setInputType(etv.getInputType()==InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE : InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
            etv.setText(etv.getInputType()==InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS ? "Text suggestions/prediction disabled!" : "Text suggestions/prediction enabled!");
        }
    }


    static {
        System.loadLibrary("hideosksuggestions");
    }

    public native void initJNI();
    public native  String stringFromJNI();
    public native void toggleAndroidOskJNI();

}
