# makefile for GigE SDK code

include ../ARCH

## Executable
EXE	= change_ip
    
$(OBJ_DIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $(VERSION) -c $< -o $@

clean: 
	rm $(EXE)

sample : $(EXE).cpp
	$(CC) $(RPATH) $(TARGET) $(CFLAGS) $(EXE).cpp -o $(EXE) $(SOLIB) $(PVLIB)

sample-static : $(EXE).cpp
	$(CC) $(RPATH) $(TARGET) -g $(CFLAGS) $(EXE).cpp $(SALIB) -o $(EXE) $(SOLIB)
	
install:
	cp -f $(EXE) $(BIN_DIR)
