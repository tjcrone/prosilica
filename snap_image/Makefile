# makefile for GigE SDK code

include ../arch/arm

# Executable
EXE	= snap_image
    
$(OBJ_DIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $(VERSION) -c $< -o $@

sample-static : $(EXE).cpp
	$(CC) $(RPATH) $(TARGET) -g $(CFLAGS) $(EXE).cpp $(SALIB) -o $(EXE) $(SOLIB)

#sample : $(EXE).cpp
#	$(CC) $(RPATH) $(TARGET) $(CFLAGS) $(EXE).cpp -o $(EXE) $(SOLIB) $(PVLIB)

clean:
	rm $(EXE)
	
#install:
#	cp -f $(EXE) $(BIN_DIR)
