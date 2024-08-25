int entry(int argc, char **argv) {
	
	window.title = STR("Minimal Game Example");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720; 
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0x6495EDff);

	while (!window.should_close) {
		reset_temporary_storage();
		
		os_update(); 
		gfx_update();
	}

	return 0;
}