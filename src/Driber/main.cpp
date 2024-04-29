#include "defs/common.h"
#include "detection.h"

UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\driber");
UNICODE_STRING DEVICE_SYMBOLIC_NAME = RTL_CONSTANT_STRING(L"\\DosDevices\\driber");

NTSTATUS OnMessage(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	print("IRQL: %d\n", KeGetCurrentIrql());

	check_all_memory();

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	DeviceObject;Irp;

	print("driber handle opened\n");
	return STATUS_SUCCESS;
}

NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	DeviceObject;Irp;

	print("driber handle closed\n");
	return STATUS_SUCCESS;
}


void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrint("driber unload!\n");
	IoDeleteDevice(DriverObject->DeviceObject);
	IoDeleteSymbolicLink(&DEVICE_SYMBOLIC_NAME);

	unload();

	DbgPrint("driber - Driver unloaded!\n");
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	print("driber entry\n");

	NTSTATUS status = 0;

	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnMessage;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnClose;

	if (!setup_cr3()) {
		return STATUS_UNSUCCESSFUL;
	}

	print("driber loaded\n");

	status = IoCreateDevice(DriverObject, 0, &DEVICE_NAME, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DriverObject->DeviceObject);
	if (NT_SUCCESS(status)) {
		print("driber device created\n");
	} else {
		print("driber error creating device\n");
		return STATUS_UNSUCCESSFUL;
	}

	status = IoCreateSymbolicLink(&DEVICE_SYMBOLIC_NAME, &DEVICE_NAME);
	if (NT_SUCCESS(status)) {
		print("driber symbolic link created\n");
	} else {
		print("driber error creating symbolic link\n");
		IoDeleteDevice(DriverObject->DeviceObject);
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}