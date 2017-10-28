# makefile for GigE SDK code

include ../arch/arm

## Executable
EXE	= change_ip
    
$(OBJ_DIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $(VERSION) -c $< -o $@

sample : $(EXE).cpp
	$(CC) $(RPATH) $(TARGET) $(CFLAGS) $(EXE).cpp -o $(EXE) $(SOLIB) $(PVLIB)

clean:
	rm $(EXE)

sample-static : $(EXE).cpp
	$(CC) $(RPATH) $(TARGET) -g $(CFLAGS) $(EXE).cpp $(SALIB) -o $(EXE) $(SOLIB)
	
install:
	cp -f $(EXE) $(BIN_DIR)
